# day17 - exec-aout：从 ramfs 加载用户程序执行

文件系统第一次"干活"：执行一个文件。构建时 NASM 把用户程序 `user/hello.asm`（平坦 a.out 式二进制，链接于 `0x80000`）编成 `hello.bin`，经 `blob.asm` 内嵌进内核镜像；启动后 `kmain` 把它作为文件 `hello` 写进 ramfs。shell 新增 `run <prog>`：从 ramfs 把文件读到物理 `0x80000`，`kernel_task_create` 开一个新任务，用 day13 的 iret 帧法（cs=0x1B、esp=0xB0000、eip=0x80000）把它送上 ring 3，随后 `wait` 收尸。

这就是 exec 的本质——**把文件内容变成一段可执行的内存现场**。测试注入 `run hello⏎`。串口输出：

```
[day17] linux011 mini shell
run: exec hello at 0x80000
[day17] hello world, exec'd from ramfs!
[day17] my pid is 1
run: hello (pid 1) exited with 0
```

## 知识点

- **a.out 平坦格式的极简版**：0.11 的用户程序是 a.out（`include/a.out.h`：header + text + data + bss + symbols），exec 时按 header 分段装填。本课的 `hello.bin` 是纯 flat binary——所有信息退化为"装到固定地址 0x80000、从 0 开始执行"两个约定，等于 header 里只剩 entry 和 load addr 两项。
- **exec 与 fork 的分工**：本课的 `run` 是"新建任务 + 装程序"一步到位；0.11 的 shell 实际是 `fork()` + `execve()` 两步——先复制再换身。把两者分开的完整版在 day19（`sys_fork` + `sys_execve`）。
- **加载地址 0x80000 与用户栈 0xB0000**：用户代码空间固定在 512KB~，栈定在 0xB0000 之上向下——由于恒等映射与无独立地址空间，这些"布局约定"代替了 0.11 里每进程独立的页目录（`copy_mem` 给每个任务 64MB 虚拟空间）。
- **exec 的返回语义**：`cmd_run` 里 `kernel_task_create(1, launch_user_80000, 'H')` 后立刻 `wait()`——launcher 任务 iret 进用户程序，用户程序 exit 后变成僵尸被收割。对比 day19：真正的 `execve` 是"改写当前任务的 iret 现场"，连 launcher 都省掉。
- **内嵌文件 vs 盘上文件**：`blob.asm` 的 `incbin` 把用户程序焊进内核镜像，`kmain` 再 `ramfs_write("hello", ...)` 存进 ramfs——内核既是加载器又是"发行渠道"。day18/19 用 initrd 把这个渠道挪到内核之外。

## 关键代码走读

（本课路径均相对 `days/day17-exec-aout/`）

`kernel/main.c` 的 `cmd_run` —— 读文件、开任务、收尸：

```c
static void cmd_run(const char *name)
{
    if (ramfs_read(name, (char *)0x80000, 8192) < 0) {
        printk("run: %s: no such file\n", name);
        return;
    }
    printk("run: exec %s at 0x80000\n", name);
    kernel_task_create(1, launch_user_80000, 'H');
    int st = 0;
    int pid = wait(&st);
    printk("run: %s (pid %d) exited with %d\n", name, pid, st);
}
```

`kernel/main.c` 的 `launch_user_80000` —— ring3 降落帧（0x80000/0xB0000）：

```asm
pushl $0x23 / $0xB0000 / $0x202 / $0x1B / $0x80000
iret                                   /* cs=0x1B, eip=0x80000 */
```

`user/hello.asm` —— 用户程序全部身家：

```nasm
[ORG 0x80000]
start:
    mov eax, 4                 ; __NR_write
    mov ebx, 1
    mov ecx, msg1
    int 0x80
    mov eax, 1                 ; __NR_exit
    int 0x80
```

`user/blob.asm` —— `incbin` 内嵌包装（`user_blob_start/end`）。

## 验收

`test/expect.txt` 五行顺序断言（`KEYS := r,u,n,spc,h,e,l,l,o,ret`）：

1. `linux011 mini shell` —— shell 就绪；
2. `run: exec hello at 0x80000` —— `run` 命令解析、ramfs 里找到 `hello`、读入 0x80000；
3. `[day17] hello world, exec'd from ramfs!` —— **用户程序在 ring3 经 write 系统调用打印**：加载地址、降权、int 0x80 三件事同时正确；
4. `[day17] my pid is 1` —— getpid 返回的任务 pid 与字符串拼接正确；
5. `run: hello (pid 1) exited with 0` —— exit(0) → 僵尸 → wait 收割，退出码回传 shell。

无像素断言。

## 对照阅读

- `source/fs/exec.c` —— 0.11 真正的 exec：`do_execve` 读 a.out header、校验 magic、按 `N_TXTADDR/N_DATAADDR` 分段装填、`set_brk`、给栈上摆 argc/argv/envp、最后改 TSS 的 cs/eip/ss/esp——本课的 `memcpy` + 固定 iret 帧是它砍掉 header 处理后的骨架。
- `source/include/a.out.h` —— `exec` 结构体（a_info/a_text/a_data/a_bss/a_entry）与宏；对照理解"flat binary = 无 header 的 a.out"。
- `source/kernel/system_call.s` + `source/lib/write.c`：用户程序里 `write(1,msg,len)` 的库封装 → `int 0x80` 的路径，`hello.asm` 的三行汇编就是它的手写版。

## 实验

1. **做一个带参数的程序**：给 `hello.asm` 加一段从固定地址（如 0x90000）读参数字符串的代码，`cmd_run` 加载后把命令行参数写到 0x90000——模拟 0.11 exec 给用户栈摆 argv 的动作。
2. **加载到别的地址**：把 `cmd_run` 与 `hello.asm` 的 `[ORG]`、`launch_user_80000` 的 eip 同时改成 `0x90000`，跑通后总结"加载地址"是哪几方的契约——再想想 0.11 为什么能每个程序用相同地址（答案：独立地址空间）。
