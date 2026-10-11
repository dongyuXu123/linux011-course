/* =====================================================================
 * day12 - kernel/sched.c：fork/exit/wait —— 任务生命周期
 * ---------------------------------------------------------------------
 * 0.11 fork() 的核心: 复制父进程的地址空间与内核栈, 父子从 fork
 * 返回点分叉, 子得到 0、父得到子 pid。我们的内核线程版: 复制父
 * 任务的内核栈(调用链原样带走), 子栈顶伪造恢复帧, 被调度时带着
 * fork()==0 回到父代码路径。
 *
 * 对照阅读: source/kernel/fork.c(copy_process)、source/kernel/exit.c
 * ===================================================================== */
#include <kernel.h>
#include <string.h>

#define NR_TASKS   4

struct task_struct {
    volatile long state;        /* 0 runnable, 1 zombie, 2 dead(已回收) */
    long counter;
    long priority;
    int  pid;
    int  exit_code;
    char name;
    uint32_t kesp;
};

enum { TASK_RUNNABLE = 0, TASK_ZOMBIE = 1, TASK_DEAD = 2 };

struct task_struct task[NR_TASKS];
struct task_struct *current = &task[0];

extern void cpu_switch_to(uint32_t *prev_esp_out, uint32_t next_esp);
extern char task_entry_stub[];
extern char fork_child_resume[];

static char task_stack[NR_TASKS][4096] __attribute__((aligned(16)));

static int next_pid = 1;

void kernel_task_create(int n, void (*fn)(void), char name)
{
    uint32_t *sp = (uint32_t *)(task_stack[n] + sizeof(task_stack[n]));
    *--sp = (uint32_t)fn;
    *--sp = (uint32_t)(uintptr_t)task_entry_stub;

    task[n].state = TASK_RUNNABLE;
    task[n].priority = 10;
    task[n].counter = task[n].priority;
    task[n].pid = next_pid++;
    task[n].exit_code = 0;
    task[n].kesp = (uint32_t)sp;
    task[n].name = name;
}

void sched_init(void)
{
    for (int n = 1; n < NR_TASKS; n++)
        task[n].state = TASK_DEAD;     /* 空槽: 可被 fork 占用 */
    task[0].state = TASK_RUNNABLE;
    task[0].priority = 10;
    task[0].counter = 10;
    task[0].pid = 0;
    task[0].name = '0';
}

/* ---- fork: C 实现, 由 kernel/fork_asm.S 包装调用 ---- */
extern char stack_top[];       /* multiboot.S: task0 内核栈顶 */

int do_fork(uint32_t caller_esp)
{
    int n;
    for (n = 1; n < NR_TASKS; n++)
        if (task[n].state == TASK_DEAD)
            break;
    if (n >= NR_TASKS)
        return -1;

    /* 复制父栈在用区间 [caller_esp, stack_top) —— 含 fork 的返回
       地址槽, 子进程 ret 后正好落在父代码 fork() 调用点下一条 */
    uint32_t used = (uint32_t)stack_top - caller_esp;
    char *cbase = task_stack[n] + sizeof(task_stack[n]);
    memcpy(cbase - used, (void *)caller_esp, used);

    /* 恢复帧: [fork_child_resume][edi][esi][ebx][ebp] */
    uint32_t *f = (uint32_t *)(cbase - used - 20);
    f[0] = (uint32_t)(uintptr_t)fork_child_resume;
    f[1] = f[2] = f[3] = f[4] = 0;

    task[n].state = TASK_RUNNABLE;
    task[n].priority = current->priority;
    task[n].counter = current->priority;
    task[n].pid = next_pid++;
    task[n].exit_code = 0;
    task[n].kesp = (uint32_t)f;
    task[n].name = current->name;
    return task[n].pid;
}

void exit(int code)
{
    printk("[day12] task pid %d exit(%d)\n", current->pid, code);
    current->exit_code = code;
    current->state = TASK_ZOMBIE;
    schedule();
    for (;;) ;
}

/* wait: 收割一个僵尸子任务(课程简化: 同名任务视为当前任务的子) */
int wait(int *status)
{
    for (;;) {
        for (int n = 1; n < NR_TASKS; n++) {
            if (task[n].state == TASK_ZOMBIE &&
                task[n].name == current->name) {
                if (status)
                    *status = task[n].exit_code;
                int pid = task[n].pid;
                task[n].state = TASK_DEAD;
                return pid;
            }
        }
        __asm__ volatile("hlt");
    }
}

/* ---- 调度(同 day11) ---- */
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
            if (task[i].state == TASK_RUNNABLE)
                task[i].counter = task[i].priority;
        return;
    }
    current->counter = current->priority;
    struct task_struct *prev = current;
    current = &task[next];
    current->counter = current->priority;
    cpu_switch_to(&prev->kesp, current->kesp);
}

void do_timer(void)
{
    if (--current->counter <= 0)
        schedule();
}
