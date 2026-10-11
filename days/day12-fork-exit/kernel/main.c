/* =====================================================================
 * day12 - kernel/main.c：fork 分叉 → 子任务 exit → 父任务 wait 收尸
 * ===================================================================== */
#include <kernel.h>

int fork(void);
void exit(int code) __attribute__((noreturn));
int wait(int *status);

static inline void sti(void)  { __asm__ volatile("sti"); }
static inline void outb(uint16_t p, uint8_t v) { __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p)); }

void kmain(void)
{
    con_init();
    trap_init();
    irq_init();
    timer_init();
    sched_init();
    sti();

    printk("[day12] parent (pid 0) forking two children\n");

    int c1 = fork();
    if (c1 == 0) {
        printk("[day12] child A running, fork returned %d\n", c1);
        exit(7);
    }
    int c2 = fork();
    if (c2 == 0) {
        printk("[day12] child B running, fork returned %d\n", c2);
        exit(42);
    }

    printk("[day12] parent got children pid=%d and pid=%d\n", c1, c2);

    int st, pid;
    pid = wait(&st);
    printk("[day12] wait: pid=%d exit_code=%d\n", pid, st);
    pid = wait(&st);
    printk("[day12] wait: pid=%d exit_code=%d\n", pid, st);
    printk("[day12] fork/exit/wait ok\n");
    outb(0xF4, 60);
    for (;;)
        __asm__ volatile("hlt");
}
