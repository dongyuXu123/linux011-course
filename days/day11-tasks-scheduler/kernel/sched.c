/* =====================================================================
 * day11 - kernel/sched.c：任务与调度器(0.11 kernel/sched.c 思想)
 * ---------------------------------------------------------------------
 * 0.11 用 TSS 硬件任务切换(ljmp 到目标 TSS, CPU 整套保存/恢复);
 * 课程实现采用等价的软件切换(switch.S): 挂起时栈上留恢复帧,
 * 调度算法本身与 0.11 一致 —— 时间片 counter 轮转。
 *
 * struct task_struct 精简自 source/include/linux/sched.h,
 * schedule()/do_timer() 对照 source/kernel/sched.c 同名函数。
 * ===================================================================== */
#include <kernel.h>

#define NR_TASKS   4

struct task_struct {
    volatile long state;        /* 0 runnable, 1 stopped */
    long counter;               /* 剩余时间片(tick) */
    long priority;              /* 时间片长度 */
    char name;
    uint32_t kesp;              /* 挂起时的内核栈指针(switch.S) */
};

enum { TASK_RUNNABLE = 0, TASK_STOPPED = 1 };

struct task_struct task[NR_TASKS];
struct task_struct *current = &task[0];

extern void cpu_switch_to(uint32_t *prev_esp_out, uint32_t next_esp);

/* ---- 任务创建(day12 换成真正的 fork) ---- */
static char task_stack[NR_TASKS][4096] __attribute__((aligned(16)));

void kernel_task_create(int n, void (*fn)(void), char name)
{
    /* 初始挂起帧: 让首次调度直接"恢复"到 fn —— 与 0.11 fork() 给
       新进程伪造 ret_from_syscall 帧是同一个思想 */
    /* 初始帧: [task_entry_stub][fn] —— 首次被 cpu_switch_to 调度时
       ret 进桩函数(开中断), 桩的 ret 再落到任务函数 */
    extern char task_entry_stub[];
    uint32_t *sp = (uint32_t *)(task_stack[n] + sizeof(task_stack[n]));
    *--sp = (uint32_t)fn;
    *--sp = (uint32_t)task_entry_stub;

    task[n].state = TASK_RUNNABLE;
    task[n].priority = 10;
    task[n].counter = task[n].priority;
    task[n].kesp = (uint32_t)sp;
    task[n].name = name;
}

void sched_init(void)
{
    task[0].state = TASK_RUNNABLE;
    task[0].priority = 10;
    task[0].counter = 10;
    task[0].name = '0';
    /* task[0] = 引导任务(kmain), 它的 kesp 在第一次切换时才保存 */
}

/* ---- 调度: 简化版 0.11 算法(时间片轮转) ---- */
void schedule(void)
{
    int i, next = -1;

    for (i = 1; i <= NR_TASKS; i++) {
        int n = (int)(current - task + i) % NR_TASKS;
        if (task[n].state == TASK_RUNNABLE) {
            next = n;
            break;
        }
    }
    if (next < 0 || next == current - task) {
        for (i = 0; i < NR_TASKS; i++)
            task[i].counter = task[i].priority;
        return;
    }
    current->counter = current->priority;
    struct task_struct *prev = current;
    current = &task[next];
    current->counter = current->priority;
    printk("[day11] switch %c -> %c\n", prev->name, current->name);
    cpu_switch_to(&prev->kesp, current->kesp);
}

/* 时钟驱动: 0.11 timer_interrupt 的核心两行 */
void do_timer(void)
{
    if (--current->counter <= 0)
        schedule();
}
