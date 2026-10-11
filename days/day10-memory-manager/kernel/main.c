/* =====================================================================
 * day10 - kernel/main.c：物理页管理验收
 * ===================================================================== */
#include <kernel.h>

struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
};

extern char _kernel_end[];

static inline void sti(void)  { __asm__ volatile("sti"); }
static inline void outb(uint16_t p, uint8_t v) { __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p)); }

#define MAX_KEEP 4096

void kmain(void)
{
    con_init();
    trap_init();
    irq_init();
    timer_init();
    sti();

    struct multiboot_info *mbi = (struct multiboot_info *)saved_mbi;
    uint32_t start = ((uint32_t)_kernel_end + 4095) & ~4095UL;
    mem_init(start, mbi->mem_upper);
    printk("[day10] paging pool ready, kernel_end=%p\n", start);
    printk("[day10] free pages at start: %d\n", mem_free_pages());

    /* 1. 分配到耗尽 */
    static uint32_t pages[MAX_KEEP];
    int n = 0;
    uint32_t p;
    while ((p = get_free_page()) != 0 && n < MAX_KEEP) {
        pages[n++] = p;
    }
    printk("[day10] pool exhausted after %d pages\n", n);
    printk("[day10] free pages now: %d\n", mem_free_pages());

    /* 2. 双重释放防护 */
    free_page(pages[0]);
    free_page(pages[0]);

    /* 3. 全部归还后再分配 → 成功 */
    for (int i = 0; i < n; i++)
        free_page(pages[i]);
    p = get_free_page();
    if (p)
        printk("[day10] re-alloc ok at %p\n", p);
    else
        printk("[day10] re-alloc FAILED\n");

    printk("[day10] mm ok\n");
    outb(0xF4, 60);
    for (;;)
        __asm__ volatile("hlt");
}
