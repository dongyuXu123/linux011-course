/* =====================================================================
 * day08 - kernel/keyboard.c：IRQ1 键盘 → 环形缓冲 → tty 行规则
 * ---------------------------------------------------------------------
 * 流程: 键盘中断(IRQ1) → 从 0x60 端口读扫描码 → 翻译成 ASCII →
 *       压入 tty 读缓冲(环形队列) → 回显。
 * main 里用 tty_getc() 阻塞读取 —— 与 0.11 "中断写队列、进程读队列"
 * 的 tty 模型一致。
 *
 * 对照阅读: source/kernel/chr_drv/keyboard.S(0.11 是大汇编表)、
 *           tty_io.c 的 read_q/canonical 模式。
 * ===================================================================== */
#include <kernel.h>

static inline uint8_t inb(uint16_t p) { uint8_t r; __asm__ volatile("inb %1,%0":"=a"(r):"Nd"(p)); return r; }
static inline void outb(uint16_t p, uint8_t v) { __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p)); }

/* ---- 环形队列(0.11 tty_queue 思想) ---- */
#define QLEN 64
struct tty_queue {
    char buf[QLEN];
    int  head, tail, count;
};
static struct tty_queue read_q;

static void q_put(struct tty_queue *q, char c)
{
    if (q->count >= QLEN)
        return;                       /* 满了就丢, 课程内核从简 */
    q->buf[q->head] = c;
    q->head = (q->head + 1) % QLEN;
    q->count++;
}

static char q_get(struct tty_queue *q)
{
    if (q->count == 0)
        return -1;
    char c = q->buf[q->tail];
    q->tail = (q->tail + 1) % QLEN;
    q->count--;
    return c;
}

/* ---- 扫描码(set-1 make code) → ASCII，只收录可打印+回车+退格 ---- */
static const char keymap[128] = {
    0, 0, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/', 0,
    '*', 0,' ', 0,   /* 0x39 空格; 其余暂不关心 */
};

static int shift_down = 0;

/* IRQ1 处理器: 由 irq.c 的 register_irq(1, ...) 挂上 */
static void keyboard_interrupt(void *regs)
{
    (void)regs;
    uint8_t sc = inb(0x60);

    if (sc & 0x80) {                  /* 断开码: 记录 shift 释放 */
        sc &= 0x7F;
        if (sc == 0x2A || sc == 0x36)
            shift_down = 0;
        return;
    }
    if (sc == 0x2A || sc == 0x36) {   /* make: shift 按下 */
        shift_down = 1;
        return;
    }
    if (sc >= 128)
        return;
    char c = keymap[sc];
    if (!c)
        return;
    if (shift_down && c >= 'a' && c <= 'z')
        c -= 32;
    q_put(&read_q, c);
    con_putc(c);                      /* 回显 */
}

void tty_init(void)
{
    read_q.head = read_q.tail = read_q.count = 0;
    outb(0x64, 0xAE);             /* 8042: 启用 PS/2 第一接口(键盘) */
    register_irq(1, keyboard_interrupt);
}

/* 非阻塞读一个键(图形循环用); 无键返回 -1 */
char tty_getc_nonblock(void)
{
    return q_get(&read_q);
}

/* 阻塞读一个键(hlt 等中断) */
char tty_getc(void)
{
    char c;
    while ((c = q_get(&read_q)) == -1)
        __asm__ volatile("hlt");
    return c;
}

/* 供 sys_read 使用 */
char tty_q_get(void)   { return tty_getc(); }
