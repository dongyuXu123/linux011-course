/* =====================================================================
 * day09 - kernel/main.c：printk/vsprintf 格式化验收
 * ===================================================================== */
#include <kernel.h>

void tty_init(void);

static inline void sti(void)  { __asm__ volatile("sti"); }
static inline void outb(uint16_t p, uint8_t v) { __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p)); }

int printk(const char *fmt, ...);

void kmain(void)
{
    con_init();
    trap_init();
    irq_init();
    timer_init();
    tty_init();
    sti();

    printk("[day09] dec=%d hex=%#x str=%s char=%c\n", -42, 42, "linux011", 'Z');
    printk("[day09] pad [%05d] [%5d] [%-5d| end\n", 42, 7, 3);
    printk("[day09] str [%5s] [%-5s] [%.2s]\n", "abc", "abc", "abcdef");
    printk("[day09] 100%% complete, vsprintf ok\n");
    outb(0xF4, 60);
    for (;;)
        __asm__ volatile("hlt");
}
