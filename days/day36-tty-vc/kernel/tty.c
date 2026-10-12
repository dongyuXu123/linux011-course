/* =====================================================================
 * day36 - kernel/tty.c：行规程(line discipline) —— 0.11 chr_drv/tty_io.c
 * ---------------------------------------------------------------------
 * 键盘字符先进规程, 不直接给读者: 退格擦字(^H)、^U 清行、回显,
 * 回车才把"整行"交给读者 —— 规范模式(canonical)的精髓: read 的
 * 单位是行而不是字符。每虚拟控制台一条规程; Alt-Fn 切焦点。
 * 对照阅读: source/kernel/chr_drv/tty_io.c read_chan/write_chan
 * ===================================================================== */
#include <kernel.h>
#include <string.h>

#define LDISC_MAX 64

struct tty_ldisc {
    char         buf[LDISC_MAX];
    int          len;
    volatile int ready;             /* 一行就绪 */
};

static struct tty_ldisc ld[2];      /* 每控制台一条 */
static int tty_focus = 0;

/* IRQ 上下文: 键盘字符入口(回显也归规程, 0.11 同) */
void tty_ldisc_putc(char c)
{
    struct tty_ldisc *L = &ld[tty_focus];
    if (c == '\n' || c == '\r') {
        con_putc('\n');
        if (L->len < LDISC_MAX - 1)
            L->buf[L->len++] = '\n';
        L->ready = 1;
    } else if (c == '\b') {          /* 擦字: 缓冲与回显同步退 */
        if (L->len > 0) {
            L->len--;
            con_putc('\b');
        }
    } else if (c == 0x15) {          /* ^U: 清行 */
        while (L->len > 0) {
            L->len--;
            con_putc('\b');
        }
    } else if (L->len < LDISC_MAX - 1) {
        L->buf[L->len++] = c;
        con_putc(c);                 /* 回显 */
    }
}

/* 阻塞读一行(0.11 read_chan 的规范模式路径) */
int tty_ldisc_read(char *out, int max)
{
    struct tty_ldisc *L = &ld[tty_focus];
    while (!L->ready)
        __asm__ volatile("hlt");     /* 任何中断醒来再看 ready */
    int n = (L->len < max) ? L->len : max;
    memcpy(out, L->buf, n);
    L->len = 0;
    L->ready = 0;
    return n;
}

/* Alt-Fn 切焦点(规程缓冲随控制台走) */
void tty_ldisc_focus(int vc)
{
    if (vc >= 0 && vc < 2)
        tty_focus = vc;
}

/* ---- 自检钩子(程序化喂字符, 等价于键盘输入) ---- */
void tty_ldisc_test_feed(char c) { tty_ldisc_putc(c); }
