# day12 - fork-exit：任务复制、退出与收尸

 Unix 三件套的内核线程版。`fork()` 复制当前任务的内核栈（整条调用链原样带走），父进程从 `fork` 调用点带着子 pid 返回，子任务被调度时经伪造的恢复帧回到同一段代码、以 `fork()==0` 分叉；`exit(code)` 把任务变成 ZOMBIE 并让出 CPU；`wait()` 收割僵尸、取回退出码后把槽位标记 DEAD。

`kmain` 的剧本：fork 两个子任务，A `exit(7)`、B `exit(42)`，父任务两次 `wait` 按顺序收尸。串口输出：

```
[day12] parent (pid 0) forking two children
[day12] child A running, fork returned 0
[day12] task pid 1 exit(7)
[day12] child B running, fork returned 0
[day12] task pid 2 exit(42)
[day12] parent got children pid=1 and pid=2
[day12] wait: pid=1 exit_code=7
[day12] wait: pid=2 exit_code=42
[day12] fork/exit/wait ok
```

## 知识点

- **fork 的"一次调用两次返回"**：父进程返回子 pid，子进程返回 0。实现上子进程从未"调用"过 fork——它的内核栈是从父进程复制来的副本，被调度时从恢复帧出发 `xor eax` 后 `ret`，恰好落回父代码 fork() 调用点的下一条、且 EAX=0。0.11 的 fork 走 int 0x80 → `copy_process` 伪造 `ret_from_syscall` 帧，思想相同。
- **栈复制的边界**：只复制**在用区间** `[caller_esp, 父栈顶)`——含 fork 的返回地址槽和上面的完整调用链。`caller_esp` 由 `fork_asm.S` 精确计算（push 4 个寄存器后 `esp+16` 正对返回地址槽），这是"子进程 ret 后回到哪里"的锚点。
- **恢复帧布局**：子栈在复制区间之下再压 5 格 `[fork_child_resume][edi][esi][ebx][ebp]`——与 `cpu_switch_to` 尾部弹出的寄存器顺序严格对应；`fork_child_resume` 清 EAX 后 `ret`，于是"fork 对子进程返回 0"不需要任何条件分支。
- **ZOMBIE 状态与收尸**：`exit()` 不直接回收任务槽——先记 `exit_code`、置 `TASK_ZOMBIE`、`schedule()` 让出 CPU 且永不返回；`wait()` 找到与父同名的僵尸，取码、置 DEAD。0.11 的 `exit.c` 里僵尸态与 `notify_parent`/`release` 的两步释放正是为了不让退出码丢失。
- **任务槽分配**：`sched_init` 把 1~3 号槽置 DEAD 表示可复用，`do_fork` 找第一个 DEAD 槽——相当于 0.11 在 `task[]` 数组里 `find_empty_process()` 找 `last_pid` 空位。

## 关键代码走读

（本课路径均相对 `days/day12-fork-exit/`）

`kernel/fork_asm.S` 的 `fork` —— 精确捕获调用者 esp：

```asm
fork:
    pushl %ebp / %ebx / %esi / %edi
    movl  %esp, %eax
    addl  $16, %eax          # eax = 调用者的 esp(栈顶是 fork 的返回地址)
    pushl %eax
    call  do_fork            # C: 槽位分配/栈复制/恢复帧
```

`kernel/sched.c` 的 `do_fork` —— 复制在用栈区间并伪造恢复帧：

```c
uint32_t used = (uint32_t)stack_top - caller_esp;
char *cbase = task_stack[n] + sizeof(task_stack[n]);
memcpy(cbase - used, (void *)caller_esp, used);
uint32_t *f = (uint32_t *)(cbase - used - 20);
f[0] = (uint32_t)fork_child_resume;    /* [恢复点][edi][esi][ebx][ebp] */
f[1] = f[2] = f[3] = f[4] = 0;
task[n].kesp = (uint32_t)f;
```

`kernel/switch.S` 的 `fork_child_resume` —— 子进程的"出生点"：

```asm
fork_child_resume:
    xorl %eax, %eax          # fork 对子进程返回 0
    popl %edi / %esi / %ebx / %ebp
    ret                      # 回到父代码 fork() 调用点下一条
```

`kernel/sched.c` 的 `exit`/`wait`：`exit` 记码置 ZOMBIE 后 `schedule()`；`wait` 轮询僵尸（无名可匹配时 `hlt` 睡眠）。

## 验收

`test/expect.txt` 九行顺序断言，完整复现 fork/exit/wait 时序：

- `parent (pid 0) forking two children` → 两次 fork 都发生在父路径；
- `child A running, fork returned 0` 与 `child B running, fork returned 0` —— 两个子任务各自被调度过，且**都以 EAX=0 分叉**（否则不会走进 if(c==0) 分支）；
- `task pid 1 exit(7)` / `task pid 2 exit(42)` —— 退出码被正确记录；
- `parent got children pid=1 and pid=2` —— 父路径拿到递增 pid；
- `wait: pid=1 exit_code=7` / `wait: pid=2 exit_code=42` —— 僵尸收割顺序与退出码传递正确；
- `fork/exit/wait ok` 收尾。

无像素断言。

## 对照阅读

- `source/kernel/fork.c` 的 `copy_process()`——0.11 版"复制任务"：`get_free_page()` 拿任务页与内核栈、复制 `task_struct`、`copy_mem` 复制页表、最后给子进程的 TSS 塞好 `eip=ret_from_syscall`、`eax=0`。差异点：0.11 是**地址空间级**复制（页表 COW 前身），课程版只复制内核栈。
- `source/kernel/exit.c` 的 `do_exit()`：置僵尸、`tell_father` 通知、`schedule()` 永不返回；`sys_waitpid()` 对应本课 `wait()`（0.11 还会处理僵儿子先于父死、信号等分支）。
- `source/kernel/system_call.s` 的 `ret_from_syscall`：0.11 子进程"出生"后第一条执行的路——与本课 `fork_child_resume` 逐句对照。

## 实验

1. **子任务里再 fork**：让 child A 在 `exit(7)` 之前再 fork 一个孙任务并 wait 它——验证槽位分配（4 个槽刚好占满）与嵌套等待；再 fork 第 5 个，确认 `do_fork` 返回 -1。
2. **观察栈复制边界**：在 `do_fork` 里把 `used` 故意少算 8 字节（`- 8`），预测并观察子任务的返回链哪一环先断（大概率表现为串口乱跳或挂死）——用失败理解 `caller_esp` 为什么必须精确。
