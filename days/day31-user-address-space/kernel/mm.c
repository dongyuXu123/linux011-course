/* =====================================================================
 * day10 - kernel/mm.c：物理页管理 —— 0.11 mm/memory.c 的核心思想
 * ---------------------------------------------------------------------
 * 0.11 把 1MB~16MB 的物理内存按 4KB 一页建 mem_map[] 位图:
 *   get_free_page(): 从高位往低位找第一个 0 项, 清零页面并返回地址
 *   free_page():     归还, 带双重释放/越界防护
 * 我们的内核由 GRUB 装到 1MB, head.S 已恒等映射 0~16MB —— 所以
 * "分配一页"只改位图, 地址立刻可用(0.11 同款)。
 *
 * 对照阅读: source/mm/memory.c 的 mem_init/get_free_page/free_page
 * ===================================================================== */
#include <kernel.h>

extern uint32_t page_dir;   /* head.S 引导页目录(线性==物理) */

#define LOW_MEM     0x100000UL                 /* 1MB 以下留给 BIOS/内核影像 */
#define PAGING_MEMORY (15UL * 1024 * 1024)     /* 恒等映射覆盖到 16MB */
#define PAGING_PAGES  (PAGING_MEMORY >> 12)    /* 3840 页 */

static unsigned char mem_map[PAGING_PAGES];

extern char _kernel_end[];    /* mk/linker.ld: PROVIDE(_kernel_end) */

static uint32_t mem_start;    /* 可分配起点(内核影像之后) */
static uint32_t mem_end;      /* 可分配终点(multiboot mem_upper 决定, 封顶16MB) */

static inline void *memset_page(void *p)
{
    unsigned char *q = p;
    for (int i = 0; i < 4096; i++)
        q[i] = 0;
    return p;
}

/* 由 main 调用: start = 内核影像结束, upper_kb = GRUB 探测的内存 */
void mem_init(uint32_t start, uint32_t upper_kb)
{
    mem_start = start;
    mem_end = LOW_MEM + PAGING_MEMORY;           /* 封顶 16MB */
    uint32_t avail_end = LOW_MEM + (uint32_t)upper_kb * 1024;
    if (avail_end < mem_end)
        mem_end = avail_end;

    for (int i = 0; i < PAGING_PAGES; i++)
        mem_map[i] = 0;
    /* 内核影像占用的页全部标记已用(0.11: for start_mem... USED) */
    for (uint32_t a = LOW_MEM; a < start; a += 4096)
        mem_map[(a - LOW_MEM) >> 12] = 1;
}

uint32_t get_free_page(void)
{
    /* 0.11 同款: 从高位往低位找, 高地址页留给后长出来的结构 */
    for (int i = PAGING_PAGES - 1; i >= 0; i--) {
        if (mem_map[i] == 0) {
            mem_map[i] = 1;
            uint32_t addr = LOW_MEM + ((uint32_t)i << 12);
            memset_page((void *)addr);
            return addr;
        }
    }
    return 0;
}

void free_page(uint32_t addr)
{
    if (addr < mem_start || addr >= mem_end || (addr & 0xFFF)) {
        printk("[day10] free_page bad addr 0x%x ignored\n", addr);
        return;
    }
    int i = (addr - LOW_MEM) >> 12;
    if (mem_map[i] == 0) {
        printk("[day10] double free at 0x%x detected!\n", addr);
        return;
    }
    mem_map[i] = 0;
}

uint32_t mem_free_pages(void)
{
    uint32_t n = 0;
    for (int i = 0; i < PAGING_PAGES; i++)
        if (!mem_map[i])
            n++;
    return n;
}

/* =====================================================================
 * day31 —— 0.11 式每进程地址空间
 * ---------------------------------------------------------------------
 * 布局对齐 source/mm/memory.c + include/linux/sched.h:
 *   线性空间: 任务 n 独占 [n*64MB, (n+1)*64MB), 经各自 LDT 的段
 *   (base=任务基址) 呈现; 内核区线性 0~16MB 恒等映射, 每个页目录
 *   都复制引导页目录的全部 PDE —— 内核与 LFB 在任何任务里可见。
 *   物理页仍取自 mem_map 池(<16MB), 由 mm_map_page 挂进任务的
 *   页表; CR3/LDTR 随任务切换(sched.c)。
 * ===================================================================== */

/* 复制引导页目录 → 新任务页目录(物理地址, 恒等映射下直接可写) */
uint32_t mm_pgdir_new(void)
{
    uint32_t dir = get_free_page();
    if (!dir)
        return 0;
    uint32_t *dst = (uint32_t *)dir;
    uint32_t *src = &page_dir;
    for (int i = 0; i < 1024; i++)
        dst[i] = src[i];
    return dir;
}

/* 线性地址挂物理页: PDE 空则先分配页表页 */
void mm_map_page(uint32_t pgdir_phys, uint32_t laddr, uint32_t phys)
{
    uint32_t *pd = (uint32_t *)pgdir_phys;
    uint32_t pde = laddr >> 22, pte = (laddr >> 12) & 0x3FF;
    if (!(pd[pde] & 1)) {
        uint32_t tbl = get_free_page();
        if (!tbl)
            return;
        pd[pde] = tbl | 0x7;           /* P|W|U —— 用户区可读写 */
    }
    uint32_t *t = (uint32_t *)(pd[pde] & ~0xFFF);
    t[pte] = phys | 0x7;
}

void mm_load_cr3(uint32_t phys)
{
    __asm__ volatile("movl %0, %%cr3" : : "r"(phys));
}

void mm_load_ldt(uint16_t sel)
{
    __asm__ volatile("lldt %%ax" : : "a"(sel));
}
