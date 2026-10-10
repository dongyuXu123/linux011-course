/* =====================================================================
 * day06 - kernel/main.c：分页已开启 + IDT 异常演示
 * ===================================================================== */
#include <stdint.h>

void con_init(void);
void con_puts(const char *s);
void con_puthex(uint32_t v);
void trap_init(void);

static inline uint32_t get_cr3(void)
{
    uint32_t v;
    __asm__ volatile("movl %%cr3, %0" : "=r"(v));
    return v;
}

static inline void outb(uint16_t p, uint8_t v) { __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p)); }
static void outb_exit(void) { outb(0xF4, 60); }

static inline uint32_t get_cr0(void)
{
    uint32_t v;
    __asm__ volatile("movl %%cr0, %0" : "=r"(v));
    return v;
}

void kmain(void)
{
    con_init();
    con_puts("[day06] head.S done: paging enabled\n");

    uint32_t cr0 = get_cr0();
    if (cr0 & 0x80000000) {
        con_puts("[day06] CR0.PG=1, page_dir at ");
        con_puthex(get_cr3() & 0xFFFFF000);
        con_puts("\n");
    } else {
        con_puts("[day06] ERROR: paging not on!\n");
    }

    /* 用 int $0 演示: 走 IDT[0] 同一条门。注意 #DE 是 fault,
       真实的 div 出错会在 iret 后重执行除法指令, 必须由内核修复;
       int 指令压入的 EIP 是下一条指令, 所以演示能直接返回继续 */
    con_puts("[day06] raising vector 0 via int $0 ...\n");
    __asm__ volatile("int $0");
    con_puts("[day06] returned from #DE handler via IDT\n");

    con_puts("[day06] all done\n");
    outb_exit();
    for (;;)
        __asm__ volatile("hlt");
}
