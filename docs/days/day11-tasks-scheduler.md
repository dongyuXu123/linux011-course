# day11 - tasks-scheduler：task_struct、时间片轮转与软件上下文切换

多任务诞生。本课搭出 `struct task_struct`（state/counter/priority/name/kesp）与调度器主干：`do_timer()` 每个时钟 tick 给当前任务扣一个时间片，扣完 `schedule()` 按 0.11 的时间片轮转算法挑下一个任务。3 个工作内核任务（A/B/C）与引导任务 0 轮流运行、各自打印迭代次数，10 tick 的时间片让串口上出现清晰的接力序列。

调度算法与 0.11 一致，但**上下文切换方式不同**：0.11 用 ljmp-to-TSS 硬件任务切换（CPU 整套保存/恢复寄存器），课程用 `switch.S` 的软件切换——挂起时把"恢复点 + callee-saved 寄存器"留在内核栈上，调度器只交换栈指针。这与现代 Linux（`__switch_to_asm`）同构。

串口输出：

```
[day11] sched demo: 3 kernel tasks, 10-tick slices
[day11] switch 0 -> A
[day11] task iter 0 running
[day11] switch A -> B
[day11] switch B -> C
[day11] switch C -> 0
[day11] all workers finished, schedule ok
```

## 知识点

- **时间片轮转（0.11 算法）**：每任务有 `priority`（时间片长度）与 `counter`（剩余 tick）。本课简化为"从当前+1 环形找第一个 runnable"；0.11 的 `schedule()` 还会遍历找 counter 最大的任务、处理所有任务 counter 耗尽时的时间片重置——`do_timer` 的 `if (--current->counter <= 0) schedule()` 两行与 0.11 `sched.c` 完全一致。
- **软件上下文切换**：`cpu_switch_to(prev_esp_out, next_esp)` 只保存 callee-saved 寄存器（ebx/esi/edi/ebp）+ 恢复点，其余寄存器由 C 调用约定保证在函数边界无意义；`esp` 本身就是"任务的身份"。对比 0.11 的 `ljmp $TSS1, $0`：CPU 一次性换掉全部寄存器并写 TSS——硬件切换慢且 Intel 自己都在长模式里废弃了它。
- **初始挂起帧 = 伪返回现场**：`kernel_task_create` 在新任务栈顶手工摆两格 `[fn][task_entry_stub]`，让首次 `cpu_switch_to` 的 `ret` 先落到 `task_entry_stub`（`sti` 后再 `ret` 进 fn）。这与 0.11 fork 给新进程伪造 `ret_from_syscall` 栈帧是同一思想——"新任务从未运行过，但看起来像刚被挂起过"。
- **sti 的位置**：从时钟中断里被切入的新任务继承了关中断状态（`cli` 由中断入口隐式产生，`iret` 之前一直关着），`task_entry_stub` 第一条指令 `sti` 保证它能被下一次 tick 抢占——漏掉它系统会在第一个任务里永久独占 CPU。
- **task[0] 的特殊性**：0 号是引导任务（跑 `kmain` 的那个上下文），`kesp` 首次被切走时才保存。0.11 的 task[0]（idle/manual_reversed）同样被特殊对待——它的 TSS 由 `gdt` 中的 0 号槽描述，且从未 fork 出来。

## 关键代码走读

（本课路径均相对 `days/day11-tasks-scheduler/`）

`kernel/switch.S` 的 `cpu_switch_to` —— 交换栈指针即切换任务：

```asm
cpu_switch_to:
    pushl %ebp / %ebx / %esi / %edi
    movl  8(%ebp), %ecx       # prev_esp_out
    pushl $1f                 # 恢复点: 回到本函数尾部
    movl  %esp, (%ecx)        # 保存当前任务的栈指针
    movl  12(%ebp), %esp      # 切到下一个任务的栈
    ret                       # 弹出它的恢复点
```

`kernel/sched.c` 的 `kernel_task_create` —— 手工摆初始帧：

```c
uint32_t *sp = (uint32_t *)(task_stack[n] + sizeof(task_stack[n]));
*--sp = (uint32_t)fn;                 /* stub 的 ret 落到任务函数 */
*--sp = (uint32_t)task_entry_stub;    /* 首次被调度的恢复点 */
task[n].kesp = (uint32_t)sp;
```

`kernel/sched.c` 的 `schedule`/`do_timer` —— 0.11 算法内核：

```c
void do_timer(void)
{
    if (--current->counter <= 0)
        schedule();
}
/* schedule(): 从 current+1 环形找 runnable → 重置双方 counter
   → printk 换人 → cpu_switch_to(&prev->kesp, current->kesp) */
```

## 验收

`test/expect.txt` 七行顺序断言：

1. `sched demo: 3 kernel tasks, 10-tick slices` —— 三个任务建好；
2. `switch 0 -> A` / `switch A -> B` / `switch B -> C` / `switch C -> 0` —— 四次切换**顺序严格**：证明每个任务恰好跑满一个时间片就把 CPU 让出（switch.S 保存/恢复错一个寄存器，接力就会断或重跑）；
3. `task iter 0 running` —— A 任务在自己的栈上真的执行了；
4. `all workers finished, schedule ok` —— 多轮轮转后所有任务完成，调度器回收时间片逻辑正确。

本课无像素断言。

## 对照阅读

- `source/kernel/sched.c` —— `schedule()`：0.11 先找 `counter` 最大的任务，找不到（都在等）时把所有任务的 counter 重置为 `counter = counter/2 + priority`；`do_timer()` 与本课逐字对应。`switch_to` 在 0.11 里是 **ljmp-to-TSS**：`include/asm/system.h` 的宏 `#define switch_to(n) { ... "ljmp %0\n" ... }`，CPU 保存当前全部现场到当前 TSS、从目标 TSS 装载——与 `kernel/switch.S` 的软件切换对照着读，是本课最有价值的比较。
- `source/include/linux/sched.h` 的 `struct task_struct`：课程版只留 state/counter/priority/kesp，0.11 里还有 `tss_struct tss` 成员——硬件切换把"任务状态"存进 TSS，软件切换把它留在内核栈上。
- `source/kernel/fork.c`：0.11 创建任务时伪造 `ret_from_syscall` 帧的做法，见 day12 的对照。

## 实验

1. **换一种调度策略**：把 `schedule()` 改成 0.11 的"找 counter 最大者"，给 A/B/C 设不同 `priority`（如 10/20/5），观察接力模式变化——验证 counter 递减与重置逻辑。
2. **删掉 sti 试试**：注释 `task_entry_stub` 的 `sti`，预测现象（第一个任务被切走后时钟中断再也无法进入，串口停在某个 switch 之后），跑一次验证——这是理解"中断状态也是现场的一部分"的最快方式。
