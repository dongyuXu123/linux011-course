# day19 - user-init：用户态 /sbin/init 与 ring3 fork/execve/wait4

第二周收官：Unix 用户态启动链成型。`/sbin/init` 是第一个真用户程序（`user/init.c`，gcc -m32 + `user/user.ld` 链接于 `0x40000`）：内核把它从 ramfs 读到 `0x40000` 后 iret 送入 ring3，它打印横幅后进入 `read` 阻塞循环，收到 `run hello` 时走完整的 **fork → execve → wait4**——不再是 day17 的"新建任务一步到位"。

本课有两个硬核改动。其一：initrd **不再用 GRUB module**（本环境 GRUB2 2.14 对 multiboot1 的 module 一律报 out of memory，源码注释有记录），改为挂第二块 IDE 盘、由新文件 `kernel/ata.c` 以 ATA PIO 轮询读入。其二：`sys_fork` 实现 **ring3 fork**——复制含 syscall 现场的整个内核栈，并把恢复帧里指向父栈的指针平移到子栈副本（do_fork 的 delta 逻辑）。

串口输出（测试注入 `run hello⏎`）：

```
[day18] initrd read from ATA disk (64KB)
[day18] initrd: added init (793 bytes)
[day18] initrd: added hello (133 bytes)
[day19] /sbin/init running in ring 3 (pid 1)
forked child pid 2
[day17] hello world, exec'd from ramfs!
[init] child 2 exited with 0
```

## 知识点

- **initrd 改道 ATA**：`kernel/ata.c` 探测主通道主/从盘（`0xA0`/`0xB0`，`ATA_CMD_IDENTIFY=0xEC`），`ata_read_sectors` 用 28 位 LBA + `0x20` 读命令，轮询状态口等 DRQ 后 `inw` 256 词/扇区。128 扇区 = 64KB initrd。QEMU 侧用 `-drive file=initrd.img,if=ide` 挂第二块盘——"从块设备装载根文件系统"恰好是 0.11 的启动路径。
- **ring3 fork 的栈复制**：`sys_fork` 记下当前 esp（此刻栈上有 int0x80 压入的完整 `pt_regs`），`do_fork(esp_now, frame, 2)` 把 `[esp_now, 父栈顶)` 整段复制到子任务栈；子任务的恢复帧只有两格 `[fork_child_resume2][regs指针]`。
- **delta 平移（本课最精的一步）**：复制的栈里可能藏着**指向父栈的指针**（如 iret 要用的 esp、或内核代码里保存的栈地址）。`do_fork` 遍历恢复帧，凡落在 `[caller_esp, 父栈顶)` 区间内的值一律 `+= delta`（子栈副本与原地址的偏移）——否则子进程一恢复就会跑回父进程的内核栈上，父子互相踩踏。
- **fork_child_resume2**：`xor eax`（fork 对子返回 0）→ `popl %esp`（换到子栈副本上被复制的 syscall 现场）→ `jmp system_call_resume` 走公共出口 `iret` 回 ring3——子进程"从 int 0x80 里醒来"，以为自己也刚做完一次 fork。
- **execve 的现场改写**：`sys_execve` 不开新任务，直接改当前 `pt_regs`：`eip=0x80000, oesp=0xB0000, cs=0x1B, eflags=0x202`，`iret` 时"变身"完成——这就是 0.11 `do_execve` 改 TSS 寄存器组的课程等价物。
- **调用号补全**：fork=2 / read=3 / execve=11 / wait4=72，全部沿用 0.11 `include/unistd.h` 的编号；`sys_read` 以行模式从 tty 队列收字节，用户态 init 因此能"scanf"。

## 关键代码走读

（本课路径均相对 `days/day19-user-init/`）

`kernel/ata.c` 的 `ata_read_sectors` —— PIO 读一扇区：

```c
outb(ATA_DRIVE, 0xE0 | ata_drive | ((lba >> 24) & 0x0F));
outb(ATA_SECCNT, 1);
outb(ATA_COMMAND, ATA_CMD_READ_PIO);
while (!((st = inb(ATA_STATUS)) & ST_DRQ)) if (st & ST_ERR) return -1;
for (int i = 0; i < 256; i++)
    w[s * 256 + i] = inw(ATA_DATA);
```

`kernel/sched.c` 的 `do_fork` —— delta 平移恢复帧：

```c
uint32_t used = current->kstack_top - caller_esp;
char *cbase = task_stack[n] + sizeof(task_stack[n]);
memcpy(cbase - used, (void *)caller_esp, used);
uint32_t delta = (uint32_t)(cbase - used) - caller_esp;
for (int i = 0; i < ndwords; i++) {
    uint32_t v = frame[i];
    if (v >= caller_esp && v < caller_esp + used)
        v += delta;                     /* 指向父栈 → 平移到子栈副本 */
    f[i] = v;
}
```

`kernel/switch.S` 的 `fork_child_resume2`：

```asm
fork_child_resume2:
    xorl %eax, %eax          # 子进程 fork 返回 0
    popl %esp                # esp 换到复制的 syscall 现场
    jmp  system_call_resume  # 公共出口 iret 进 ring3
```

`user/init.c` 的主循环：`read` 一行 → `fork()` → 子 `execve(line+4)` → 父 `wait4(pid,&st)`。

## 验收

`test/expect.txt` 七行顺序断言：

1. `initrd read from ATA disk (64KB)` —— ATA 探测与 128 扇区读取成功；
2. `initrd: added init (793 bytes)` / `added hello (133 bytes)` —— romfs-lite 解包出两个文件且长度精确；
3. `/sbin/init running in ring 3 (pid 1)` —— init 程序经 getpid 自证身份；
4. `forked child pid 2` —— 用户态 fork 返回子 pid（父视角）；
5. `[day17] hello world, exec'd from ramfs!` —— execve 后子进程在 ring3 打印（`hello.asm` 沿用 day17 前缀）；
6. `[init] child 2 exited with 0` —— wait4 收尸拿到退出码，全链路闭环。

无像素断言。

## 对照阅读

- `source/init/main.c` 的 `init()`：0.11 在 `move_to_user_mode` 后 fork 出的第一个任务做 setup/打开 /dev/tty/fork-exec `/bin/sh`——本课 init 的 `read`+`run` 循环是它的骨架版。
- `source/kernel/fork.c` 的 `copy_process` + `copy_mem`：0.11 复制页表（写时复制前的全量复制）；本课复制的是**内核栈**——粒度不同，"子进程从复制的现场里醒来"的思想一致。
- `source/kernel/sys.c` 的 `sys_fork/sys_execve/sys_waitpid` 与 `source/kernel/exit.c`：系统调用层的对应；`source/fs/exec.c` 的 `do_execve` 是"改 iret 现场"的 0.11 版。
- `source/kernel/blk_drv/hd.c`：0.11 硬盘驱动走中断 + request 队列；`ata.c` 的轮询版与之对照（状态位/LBA/扇区计数概念一一对应）。

## 实验

1. **删掉 delta 平移**：把 `do_fork` 的 `v += delta` 注释掉，预测现象（子进程恢复时 esp 指向父栈，两个任务共用一块栈、现场互毁，串口大概率乱码或挂死），跑一次验证——这是理解"内核栈也是任务私有资源"的最深刻实验。
2. **让 init 支持第二条命令**：给 `init.c` 加 `pid` 命令（getpid 打印），重新构建 initrd 后注入 `pid⏎`——走一遍"改用户程序 → 重打 initrd → 重跑测试"的发布链路。
