/* =====================================================================
 * day08 - kernel/keyboard.c：IRQ1 键盘驱动(day36 接入行规程)
 * ---------------------------------------------------------------------
 * 流程: 键盘中断(IRQ1) → 从 0x60 端口读扫描码 → 翻译 ASCII →
 *       交给 tty 行规程(缓冲+编辑+回显, tty.c)。
 * day36 新增: Alt+F1/F2 切换虚拟控制台(规程焦点随控制台走)。
 *
 * 对照阅读: source/kernel/chr_drv/keyboard.S(0.11 是大汇编表)、
 *           tty_io.c 的 read_q/canonical 模式。
 * ===================================================================== */
#include <kernel.h>

static inline uint8_t inb(uint16_t p) { uint8_t r; __asm__ volatile("inb %1,%0":"=a"(r):"Nd"(p)); return r; }
static inline void outb(uint16_t p, uint8_t v) { __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p)); }

/* ---- 扫描码(set-1 make code) → ASCII，只收录可打印+回车+退格 ---- */
static const char keymap[128] = {
    0, 0, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/', 0,
    '*', 0,' ', 0,   /* 0x39 空格; 其余暂不关心 */
};

static int shift_down = 0, alt_down = 0;

/* IRQ1 处理器: 由 irq.c 的 register_irq(1, ...) 挂上 */
static void keyboard_interrupt(void *regs)
{
    (void)regs;
    uint8_t sc = inb(0x60);

    if (sc & 0x80) {                  /* 断开码: 记录修饰键释放 */
        sc &= 0x7F;
        if (sc == 0x2A || sc == 0x36)
            shift_down = 0;
        if (sc == 0x38)
            alt_down = 0;
        return;
    }
    if (sc == 0x2A || sc == 0x36) {   /* make: shift 按下 */
        shift_down = 1;
        return;
    }
    if (sc == 0x38) {                 /* Alt */
        alt_down = 1;
        return;
    }
    if (alt_down && (sc == 0x3B || sc == 0x3C)) {   /* Alt-F1/F2 */
        tty_ldisc_focus(sc == 0x3B ? 0 : 1);
        con_switch(sc == 0x3B ? 0 : 1);
        return;
    }
    if (sc >= 128)
        return;
    char c = keymap[sc];
    if (!c)
        return;
    if (shift_down && c >= 'a' && c <= 'z')
        c -= 32;
    tty_ldisc_putc(c);                /* 规程收编: 缓冲+编辑+回显 */
}

void tty_init(void)
{
    outb(0x64, 0xAE);             /* 8042: 启用 PS/2 第一接口(键盘) */
    register_irq(1, keyboard_interrupt);
}
