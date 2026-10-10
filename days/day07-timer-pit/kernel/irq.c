/* =====================================================================
 * day07 - kernel/irq.c：8259A 重映射 + IRQ 分发表
 * 对照阅读: source/kernel/sched.c 里 timer_interrupt 挂接、
 *           0.11 没有独立 irq.c(它在各驱动里开中断), 这里集中管理。
 * ===================================================================== */
#include <kernel.h>



/* 与 traps.c 相同的门/描述符操作 —— 复制声明以保持每课自包含 */
struct idt_entry {
    uint16_t off_lo;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  type_attr;
    uint16_t off_hi;
} __attribute__((packed));
struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));
extern struct idt_entry idt[256];
extern struct idt_ptr   idtp;

typedef void (*irq_handler_t)(void *regs);
extern const uint32_t irq_stub_table[16];
static irq_handler_t irq_handlers[16];

static inline void outb(uint16_t p, uint8_t v) { __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p)); }
static inline uint8_t inb(uint16_t p) { uint8_t r; __asm__ volatile("inb %1,%0":"=a"(r):"Nd"(p)); return r; }

void register_irq(int irq, irq_handler_t fn)
{
    irq_handlers[irq] = fn;
}

/* 发 EOI: 从片(IRQ8~15)要发两次 */
static void irq_eoi(int irq)
{
    if (irq >= 8)
        outb(0xA0, 0x20);
    outb(0x20, 0x20);
}

/* do_irq: irq_entry.S 的公共出口调用 */
void do_irq(struct pt_regs *regs)
{
    int irq = (int)regs->vector - 0x20;
    if (irq < 0 || irq >= 16) {
        con_puts("[day07] bogus irq vector!\n");
        irq_eoi(0);
        return;
    }
    if (irq_handlers[irq])
        irq_handlers[irq](regs);
    irq_eoi(irq);
}

/* 8259 重映射: 默认占用 0x08~0x0F 会撞掉 CPU 异常, 挪到 0x20~0x2F */
void pic_init(void)
{
    outb(0x20, 0x11);          /* ICW1: 边沿触发, 级联, ICW4 */
    outb(0xA0, 0x11);
    outb(0x21, 0x20);          /* ICW2: 主片向量基址 0x20 */
    outb(0xA1, 0x28);          /* ICW2: 从片向量基址 0x28 */
    outb(0x21, 0x04);          /* ICW3: 主片 IR2 接从片 */
    outb(0xA1, 0x02);
    outb(0x21, 0x01);          /* ICW4: 8086 模式 */
    outb(0xA1, 0x01);
    outb(0x21, 0xFF & ~(1 << 0) & ~(1 << 2)); /* 放开 IRQ0(时钟)与 IRQ2(级联) */
    outb(0xA1, 0xFF);
}

/* 把向量 0x20~0x2F 装进 IDT 并挂上 IRQ0 */
void irq_init(void)
{
    extern void trap_install_gate(int vec, uint32_t handler);
    for (int i = 0; i < 16; i++)
        trap_install_gate(0x20 + i, irq_stub_table[i]);
    pic_init();
}
