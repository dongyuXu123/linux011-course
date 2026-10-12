/* =====================================================================
 * day06 - kernel/traps.c：IDT 装载与异常处理(C 侧)
 * 对照阅读: source/kernel/traps.c 与 source/include/asm/system.h 的
 * set_trap_gate / _set_gate —— 门描述符布局完全一致。
 * ===================================================================== */
#include <kernel.h>


/* 中断门: type_attr 0x8E = present | DPL0 | 32位 interrupt gate */
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

struct idt_entry idt[256];
struct idt_ptr   idtp;

void trap_install_gate(int vec, uint32_t handler)
{
    idt[vec].off_lo    = handler & 0xFFFF;
    idt[vec].selector  = 0x08;
    idt[vec].zero      = 0;
    idt[vec].type_attr = 0x8E;
    idt[vec].off_hi    = handler >> 16;
}

extern const uint32_t trap_stub_table[32];


static const char *trap_name(uint32_t v)
{
    static const char *names[32] = {
        "#DE divide error", "#DB debug", "NMI", "#BP breakpoint",
        "#OF overflow", "#BR bound range", "#UD invalid opcode", "#NM device n/a",
        "#DF double fault", "coproc seg", "#TS invalid TSS", "#NP not present",
        "#SS stack fault", "#GP general prot", "#PF page fault", "reserved",
        "#MF x87", "#AC alignment", "#MC machine", "#XM SIMD",
        "#VE virtualization", "#CP control prot", "reserved", "reserved",
        "reserved", "reserved", "reserved", "reserved",
        "reserved", "reserved", "reserved", "reserved",
    };
    return v < 32 ? names[v] : "IRQ/other";
}

/* day06 只需要"接住异常、报告、返回"。day32 起这里长成 0.11 traps.c:
   #PF 先交给 do_no_page 按需调页; 补不了的才是真非法访问, 杀任务
   (0.11 do_no_page 返回后 get_base/查页表失败路径的 send_sig 对应物) */
static inline uint32_t get_cr2(void)
{
    uint32_t v;
    __asm__ volatile("movl %%cr2, %0" : "=r"(v));
    return v;
}

extern int do_no_page(uint32_t addr, uint32_t err);

void trap_dispatch(struct pt_regs *regs)
{
    uint32_t vec = regs->vector;
    if (vec == 14) {
        uint32_t cr2 = get_cr2();
        if (!(regs->err & 1) && do_no_page(cr2, regs->err))
            return;                       /* 页已补上, 重执行即可 */
        static uint32_t pf_printed = 0;
        if (pf_printed++ < 3) {
            printk("[day32] bad page fault: cr2=%x eip=%x cs=%x err=%x"
                   " cur=%d\n", cr2, regs->eip, regs->cs, regs->err,
                   (int)(current - task));
        }
        /* 课程内核的"发 SIGSEGV": 标僵尸, 调度走人 */
        printk("[day32] killing pid %d\n", current->pid);
        current->exit_code = 139;         /* 128+11, 姿态上等价 SIGSEGV */
        current->state = TASK_ZOMBIE;
        schedule();
        return;
    }
    con_puts("[day06] exception ");
    con_puthex(vec);
    con_puts(" (");
    con_puts(trap_name(vec));
    con_puts(") handled, returning\n");
}

void trap_init(void)
{
    for (int i = 0; i < 256; i++) {
        uint32_t h = (i < 32) ? trap_stub_table[i] : 0;
        idt[i].off_lo    = h & 0xFFFF;
        idt[i].selector  = 0x08;
        idt[i].zero      = 0;
        idt[i].type_attr = 0x8E;
        idt[i].off_hi    = h >> 16;
    }
    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uint32_t)idt;
    __asm__ volatile("lidt %0" : : "m"(idtp));
}
