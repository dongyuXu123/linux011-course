/* =====================================================================
 * day08 - kernel/main.c：中断驱动的交互 —— 从"能打印"到"能听"
 * ===================================================================== */
#include <kernel.h>

void tty_init(void);
char tty_getc(void);

static inline void sti(void)  { __asm__ volatile("sti"); }
static inline void outb(uint16_t p, uint8_t v) { __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p)); }

void kmain(void)
{
    con_init();
    con_puts("[day08] console ready, waiting for keystrokes\n");

    trap_init();
    irq_init();
    timer_init();
    tty_init();          /* IRQ1 → read_q */
    sti();

    /* 读一行(处理退格), 回车后报告 */
    char line[32];
    int n = 0;
    for (;;) {
        char c = tty_getc();
        if (c == '\n') {
            line[n] = 0;
            con_puts("\n[day08] line='");
            con_puts(line);
            con_puts("'\n");
            break;
        }
        if (c == '\b') {           /* 退格已在 con_putc 里回显擦除 */
            if (n > 0)
                n--;
            continue;
        }
        if (n < 31)
            line[n++] = c;
    }

    con_puts("[day08] keyboard+tty ok\n");
    outb(0xF4, 60);
    for (;;)
        __asm__ volatile("hlt");
}
