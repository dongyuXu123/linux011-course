/* =====================================================================
 * day07 - kernel/main.c：打开中断，数心跳
 * ===================================================================== */
#include <kernel.h>

void con_init(void);
void con_puts(const char *s);
void con_puthex(uint32_t v);
void trap_init(void);
void irq_init(void);
void timer_init(void);
extern volatile uint32_t jiffies;

static inline void sti(void)  { __asm__ volatile("sti"); }
static inline void outb(uint16_t p, uint8_t v) { __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p)); }

void kmain(void)
{
    con_init();
    con_puts("[day07] enabling PIC remap + PIT (100Hz)\n");

    trap_init();      /* IDT: CPU 异常 0~31 */
    irq_init();       /* IDT: IRQ 0x20~0x2F + 8259 重映射 */
    timer_init();     /* IRQ0 → jiffies++ */

    sti();            /* 从这一刻起, 内核真正"活"了 */
    con_puts("[day07] interrupts on, waiting 3 seconds...\n");

    while (jiffies < 300)
        __asm__ volatile("hlt");   /* 空闲时让 CPU 睡觉, 等心跳叫醒 */

    con_puts("[day07] jiffies=");
    con_puthex(jiffies);
    con_puts(" heartbeat ok\n");
    outb(0xF4, 60);
    for (;;)
        __asm__ volatile("hlt");
}
