# day13 - syscall-ring3：int 0x80 系统调用门与第一个 ring 3 程序

特权级隔离上线。本课补齐三块硬件设施：GDT 增加用户段（0x18 代码/0x20 数据，DPL3）与系统 TSS（0x28 槽，ring3 中断时 CPU 换栈用）；IDT 的 0x80 号门设为 DPL3（`type_attr=0xEF`），用户态可用 `int 0x80` 陷入内核；`system_call` 入口沿用 0.11 的 ABI——EAX 放调用号（exit=1/write=4/getpid=20，出自 `source/include/unistd.h`），EBX/ECX/EDX 放参数，返回值走 EAX。

内核把 NASM 编译的用户程序（`user/user.asm`，链接于 `0x40000`）拷到物理 `0x40000`，摆好 iret 帧把执行流"踹进"ring 3；用户程序经 write 打印问候、getpid 报身份、exit(0) 退出，内核 wait 收尸。串口输出：

```
[day13] kernel ready, launching user program via int 0x80 ABI
[day13] user program copied to 0x40000, dropping to ring 3
[day13] hello from ring 3! write syscall ok
[day13] getpid says my pid is 1
[day13] user task called exit(0)
[day13] reaped user task pid=1 exit=0
[day13] syscall ABI ok
```

## 知识点

- **ring0/ring3 的门**：中断门 `type_attr` 高半字节的 DPL 决定"谁能触发"。异常门 DPL0（0x8E），用户态触碰即 #GP；0x80 门 DPL3（0xEF）专放行系统调用——这是 0.11 `system_call.s` 的 `set_intr_gate(0x80, &system_call)` 加手工改 DPL 的现代重演。
- **TSS 的真正用途**：ring3 代码触发中断/异常时，CPU 必须切到内核栈——从哪拿？`tr` 指向的 TSS 的 `ss0:esp0`。课程只在 `tss_install` 里回填 GDT 0x28 槽描述符（type 0x89，限长 103 字节）并 `ltr 0x28`；`esp0` 由调度器在每次切换时更新（day19 的 `sched.c` 可见）。0.11 同样在每次 `switch_to` 后 `set_tss_desc` + `ltr`。
- **iret 降权帧**：从 ring0 `iret` 到 ring3 时，栈上要摆 5 元组 `[用户 ss][用户 esp][eflags][用户 cs][eip]`。CPU 校验目标 CS 的 RPL≥DPL 才放行；且**降权会校验数据段寄存器**——所以入口先把 DS/ES/FS/GS 换成用户选择符 `0x23` 再 iret（0.11 `move_to_user_mode` 宏干的就是这件事）。
- **用户程序布局**：NASM `-f bin` 平坦二进制，`[ORG 0x40000]`，经 `blob.asm` 的 `incbin` 包成内核内嵌符号 `user_blob_start/end`——加载器只是 `memcpy` 到 `0x40000`。0.11 的 a.out 用户程序（`fs/exec.c`）多了 header 解析与 bss 清零，flat 版本把这些约定都固定死。
- **系统调用的返回路径**：`system_call` 压栈现场后 `call syscall_dispatch`，C 返回值在 EAX，用 `movl %eax, 44(%esp)` **覆写栈上保存的用户 EAX**，再 `iret` 回 ring3——`44(%esp)` 这个偏移是本课 pt_regs 布局的活说明书。

## 关键代码走读

（本课路径均相对 `days/day13-syscall-ring3/`）

`kernel/syscall.c` 的 `syscall_init` —— DPL3 门 + TSS：

```c
tss_install();                        /* GDT 0x28 + ltr */
trap_install_gate(0x80, (uint32_t)system_call);
idt[0x80].type_attr = 0xEF;           /* DPL3: ring3 可以 int 0x80 */
```

`kernel/syscall_entry.S` 的 `system_call` —— 返回值覆写：

```asm
call  syscall_dispatch     # C: 返回值在 eax
addl  $4, %esp
movl  %eax, 44(%esp)       # 覆写保存的 eax → 带回用户态
...
iret
```

`kernel/main.c` 的 `user_task_entry` —— 降权 iret 帧：

```asm
movl $0x23, %eax / %ds ...           /* 数据段先换用户选择符 */
pushl $0x23                          /* 用户 ss */
pushl $0x7F000                       /* 用户 esp */
pushl $0x202                         /* eflags: IF=1 */
pushl $0x1B                          /* 用户 cs = 0x18|RPL3 */
pushl $0x40000                       /* eip */
iret
```

`user/user.asm` 的调用样例：`mov eax,4 / mov ebx,1 / mov ecx,msg / int 0x80`。

## 验收

`test/expect.txt` 七行顺序断言：

1. `kernel ready, launching user program via int 0x80 ABI` —— 初始化完成、用户任务已建；
2. `user program copied to 0x40000, dropping to ring 3` —— 拷贝与 iret 降权路径启动（GDT 用户段/TSS/门任一配错，此处即三重故障）；
3. `hello from ring 3! write syscall ok` —— ring3 → int0x80 → sys_write → con_putc 全链路通；
4. `getpid says my pid is 1` —— 调用号 20 分发正确、`current->pid` 传递正确（用户程序把返回的 EAX 加 '0' 拼进字符串）；
5. `user task called exit(0)` / `reaped user task pid=1 exit=0` —— sys_exit 置僵尸 + 内核 wait 收尸闭环；
6. `syscall ABI ok` 收尾。

无像素断言。

## 对照阅读

- `source/kernel/system_call.s` —— 0.11 的系统调用入口：`_system_call` 比较 `eax` 与 `NR_syscalls`、`call _sys_call_table(,%eax,4)`、`movl %eax,EAX(%esp)` 后 `iret`——本课 `44(%esp)` 的覆写就是 `EAX(%esp)` 的课程版坐标。
- `source/kernel/sched.c` 的 `tss` 段与 `source/include/asm/system.h` 的 `_set_tss_desc`：0.11 在 `sched_init` 里给 0 号任务的 TSS 装描述符并 `ltr`——本课 `tss_install` 对应。
- `source/init/main.c` 的 `move_to_user_mode()` 宏：摆 iret 帧降权的祖师爷；`source/include/unistd.h` 的 `__NR_*` 调用号本课原样沿用。
- `source/fs/exec.c`：0.11 真正的 exec（a.out header 解析）在 day17/19 对照。

## 实验

1. **把 0x80 门改成 DPL0**：`idt[0x80].type_attr = 0x8E;`，预测现象（用户态 `int 0x80` 触发 #GP(13)，被 day06 的 trap 框架接住打印 "general prot"），跑一次验证——用一条属性字节理解 DPL 的全部意义。
2. **加一个 sys_time**：在 `syscall.c` 里加调用号 13（0.11 的 `__NR_time`），返回 `jiffies/100`；用户程序里调用并把秒数打印出来——完整走一遍"加系统调用"的五步（调用号/分发/实现/用户 ABI/测试）。
