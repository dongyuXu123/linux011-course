/* =====================================================================
 * day11 - kernel/main.c：三个内核任务在时钟驱动下轮转
 * ===================================================================== */
#include <kernel.h>

static volatile int workers_done = 0;

static void worker(void)
{
    /* current->name 由调度器视角读; 任务体里迭代 3 次,
       每次打一行后自旋等下一个时间片(被打断时 eip 停在循环里) */
    for (int iter = 0; iter < 3; iter++) {
        printk("[day11] task iter %d running\n", iter);
        for (volatile long i = 0; i < 2000000; i++)
            ;                        /* 自旋 → 必然被时钟抢占 */
    }
    workers_done++;
    while (1)
        __asm__ volatile("hlt");
}

static inline void sti(void)  { __asm__ volatile("sti"); }
static inline void outb(uint16_t p, uint8_t v) { __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p)); }

void kmain(void)
{
    con_init();
    trap_init();
    irq_init();
    timer_init();        /* IRQ0 → do_timer: 时间片与调度的心脏 */
    printk("[day11] sched demo: 3 kernel tasks, 10-tick slices\n");

    kernel_task_create(1, worker, 'A');
    kernel_task_create(2, worker, 'B');
    kernel_task_create(3, worker, 'C');
    sched_init();

    sti();
    while (workers_done < 3)
        __asm__ volatile("hlt");     /* task0 空闲: 睡到时钟叫醒 */

    printk("[day11] all workers finished, schedule ok\n");
    outb(0xF4, 60);
    for (;;)
        __asm__ volatile("hlt");
}
