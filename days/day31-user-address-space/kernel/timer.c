/* =====================================================================
 * day07 - kernel/timer.c：8253 PIT 时钟节拍
 * 对照阅读: source/kernel/sched.c 的 timer_interrupt —— 0.11 在这里
 * 做 jiffies++ 和时间片统计；day11 的调度器就从这一天长出来。
 * ===================================================================== */
#include <kernel.h>

static inline void outb(uint16_t p, uint8_t v) { __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p)); }

volatile uint32_t jiffies = 0;       /* 内核心跳, 100Hz */

#define LATCH 1193180 / 100          /* 计数初值: 1.19318MHz / 100Hz */

/* IRQ0 处理器 —— 0.11 的 timer_interrupt 雏形 */
void do_timer(void);   /* sched.c: 时间片与调度 */

static void timer_interrupt(void *regs)
{
    (void)regs;
    jiffies++;
    if (jiffies == 100)
        printk("[day11] 1s heartbeat\n");
    if (jiffies == 200)
        printk("[day11] 2s heartbeat\n");
    do_timer();          /* 时钟节拍 → 可能发生任务切换 */
}

void timer_init(void)
{
    register_irq(0, timer_interrupt);
    outb(0x43, 0x36);              /* 通道0, 方式3(方波), 先低后高 */
    outb(0x40, LATCH & 0xFF);
    outb(0x40, (LATCH >> 8) & 0xFF);
}
