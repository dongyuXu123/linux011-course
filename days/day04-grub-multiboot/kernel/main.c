/* =====================================================================
 * day04 - kernel/main.c：第一个 C 语言内核
 * ---------------------------------------------------------------------
 * 学习点:
 *   1. freestanding C —— 没有 libc，一切自己动手(先造串口输出)
 *   2. Multiboot 信息块: GRUB 放在内存里的"交接清单"，
 *      flags bit0 置位时 mem_lower/mem_upper 有效(单位 KB)
 *   3. isa-debug-exit: 向端口 0xF4 写一个数即可让 QEMU 干净退出，
 *      课程测试全靠它(退出码 = 值*2+1，写 60 → 退出码 121)
 * ===================================================================== */

#include <stdint.h>

#define COM1 0x3F8

static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t v;
    __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

/* ---- 串口驱动: day01 MBR 里那段的 C 版 ---- */
static void serial_init(void)
{
    outb(COM1 + 1, 0x00);   /* 关中断 */
    outb(COM1 + 3, 0x80);   /* DLAB=1 */
    outb(COM1 + 0, 0x01);   /* 除数=1 → 115200 */
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);   /* 8N1 */
    outb(COM1 + 2, 0xC7);   /* FIFO */
    outb(COM1 + 4, 0x0B);   /* DTR+RTS+OUT2 */
}

static void serial_putc(char c)
{
    while (!(inb(COM1 + 5) & 0x20))
        ;
    outb(COM1, (uint8_t)c);
}

static void serial_puts(const char *s)
{
    while (*s)
        serial_putc(*s++);
}

/* ---- 极简十六进制打印(后面课程会换成完整 printk) ---- */
static void serial_puthex(uint32_t v)
{
    static const char hex[] = "0123456789abcdef";
    serial_puts("0x");
    for (int i = 28; i >= 0; i -= 4)
        serial_putc(hex[(v >> i) & 0xF]);
}

/* ---- Multiboot 信息块(只声明本课用到的字段) ----
 * 完整定义见 GRUB 手册 "Boot information format"，
 * 与 0.11 里 setup.s 收集后放在 0x90000 的参数块角色相同。 */
struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower;   /* 0~640KB 区间的可用内存(KB) */
    uint32_t mem_upper;   /* 1MB 以上的可用内存(KB) */
    /* ... 其余字段本课用不到 */
};

void kmain(uint32_t magic, uint32_t addr)
{
    serial_init();

    serial_puts("[day04] booted by GRUB, entered kmain\n");

    if (magic == 0x2BADB002) {
        serial_puts("[day04] multiboot magic ok, info at ");
        serial_puthex(addr);
        serial_puts("\n");
    } else {
        serial_puts("[day04] BAD magic! not multiboot\n");
    }

    struct multiboot_info *mbi = (struct multiboot_info *)addr;
    if (mbi->flags & (1 << 0)) {
        serial_puts("[day04] mem_lower=");
        serial_puthex(mbi->mem_lower);
        serial_puts("KB mem_upper=");
        serial_puthex(mbi->mem_upper);
        serial_puts("KB\n");
    }

    serial_puts("[day04] exiting qemu via port 0xF4\n");
    outb(0xF4, 60);          /* isa-debug-exit: QEMU 退出码 121 */

    for (;;)
        __asm__ volatile("hlt");
}
