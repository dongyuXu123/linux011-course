/* =====================================================================
 * day05 - kernel/main.c：控制台演示(滚动/光标/镜像)
 * ===================================================================== */
#include <stdint.h>

void con_init(void);
void con_puts(const char *s);

static void puts_dec(char *buf, int v)   /* 小工具: int→十进制字符串 */
{
    char tmp[12];
    int i = 0, neg = v < 0;
    if (neg) v = -v;
    do { tmp[i++] = '0' + v % 10; v /= 10; } while (v);
    if (neg) buf[0] = '-', buf++;
    for (int j = 0; j < i; j++) buf[j] = tmp[i - 1 - j];
    buf[i] = 0;
}

static inline void outb(uint16_t p, uint8_t v) { __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p)); }

void kmain(void)
{
    con_init();
    con_puts("[day05] VGA text console ready (80x25)\n");

    for (int i = 1; i <= 40; i++) {       /* 刷 40 行逼出滚动 */
        char line[32] = "[day05] scroll line ";
        char num[12], *p = line;
        while (*p) p++;
        puts_dec(num, i);
        char *q = num;
        while (*q) *p++ = *q++;
        *p++ = '\n'; *p = 0;
        con_puts(line);
    }

    con_puts("[day05] scroll test done, cursor should be at bottom\n");
    outb(0xF4, 60);
    for (;;)
        __asm__ volatile("hlt");
}
