/* =====================================================================
 * day05 - kernel/console.c：VGA 文本控制台(80x25) + 串口镜像
 * ---------------------------------------------------------------------
 * 显存 0xB8000，每字符 2 字节: [ASCII][属性]。写满 25 行就整屏上滚。
 * 每写一个字符同时镜像到 COM1 —— 课程自动化测试的通道。
 * day08 接上键盘后它升级为 tty；对照 source/kernel/chr_drv/console.c
 * ===================================================================== */
#include <kernel.h>

#define COM1        0x3F8
#define VGA_TEXT    0xB8000
#define COLS        80
#define ROWS        25

static inline void outb(uint16_t p, uint8_t v) { __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p)); }
static inline uint8_t inb(uint16_t p) { uint8_t r; __asm__ volatile("inb %1,%0":"=a"(r):"Nd"(p)); return r; }

static uint16_t *vram = (uint16_t *)VGA_TEXT;
static int cur_row, cur_col;
static uint8_t cur_attr = 0x07;   /* 黑底白字 */

/* ---- 串口(day01/day04 的 C 版，con_putc 每字符同步镜像) ---- */
static void serial_init(void)
{
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x01);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}

static void serial_putc(char c)
{
    while (!(inb(COM1 + 5) & 0x20))
        ;
    outb(COM1, (uint8_t)c);
}

/* ---- 硬件光标: CRTC 端口 0x3D4/0x3D5 ---- */
static void con_update_cursor(void)
{
    uint16_t pos = cur_row * COLS + cur_col;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)(pos >> 8));
}

static void con_scroll(void)
{
    for (int i = 0; i < (ROWS - 1) * COLS; i++)
        vram[i] = vram[i + COLS];
    for (int i = (ROWS - 1) * COLS; i < ROWS * COLS; i++)
        vram[i] = (uint16_t)(cur_attr << 8) | ' ';
}

void con_init(void)
{
    serial_init();
    cur_row = cur_col = 0;
    for (int i = 0; i < ROWS * COLS; i++)
        vram[i] = (uint16_t)(cur_attr << 8) | ' ';
    con_update_cursor();
}

static int con_serial_only = 0;
void con_serial_only_set(void) { con_serial_only = 1; }

void con_putc(char c)
{
    serial_putc(c);                       /* 一切输出镜像到串口 */
    if (con_serial_only)
        return;
    if (c == '\n') {
        cur_col = 0;
        cur_row++;
    } else if (c == '\r') {
        cur_col = 0;
    } else if (c == '\b') {
        if (cur_col > 0) { cur_col--; vram[cur_row*COLS+cur_col] = (uint16_t)(cur_attr<<8) | ' '; }
    } else {
        vram[cur_row * COLS + cur_col] = (uint16_t)(cur_attr << 8) | (uint8_t)c;
        cur_col++;
        if (cur_col >= COLS) { cur_col = 0; cur_row++; }
    }
    if (cur_row >= ROWS) { con_scroll(); cur_row = ROWS - 1; }
    con_update_cursor();
}

void con_puts(const char *s)
{
    while (*s)
        con_putc(*s++);
}

/* 十六进制输出(day06+ 常用) */
void con_puthex(uint32_t v)
{
    static const char hex[] = "0123456789abcdef";
    con_puts("0x");
    for (int i = 28; i >= 0; i -= 4)
        con_putc(hex[(v >> i) & 0xF]);
}

/* 清屏并归位光标 */
void con_clear(void)
{
    cur_row = cur_col = 0;
    for (int i = 0; i < ROWS * COLS; i++)
        vram[i] = (uint16_t)(cur_attr << 8) | ' ';
    con_update_cursor();
}
