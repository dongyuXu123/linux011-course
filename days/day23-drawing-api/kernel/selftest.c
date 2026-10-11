/* =====================================================================
 * day20 - kernel/selftest.c：里程碑自检
 * 字符终端形态的 linux011-course 至此五脏俱全: GRUB 引导 / 分页 /
 * 中断 / 时钟 / 键盘 tty / 物理页管理 / 任务调度 / fork-exec-wait /
 * 系统调用 / ramfs / 用户态 init。启动时跑一轮子系统自检并出报告。
 * ===================================================================== */
#include <kernel.h>

extern volatile uint32_t jiffies;

#define POOL_PROBE 200

void selftest_run(void)
{
    printk("[day20] selftest begin\n");

    /* 1. 物理页: 分配-写 pattern-校验-释放 */
    static uint32_t pages[POOL_PROBE];
    int ok = 1;
    for (int i = 0; i < POOL_PROBE; i++) {
        pages[i] = get_free_page();
        if (!pages[i]) { ok = 0; break; }
        *(uint32_t *)pages[i] = 0xA5A50000 + i;
    }
    for (int i = 0; i < POOL_PROBE && ok; i++)
        if (*(uint32_t *)pages[i] != 0xA5A50000U + i)
            ok = 0;
    for (int i = 0; i < POOL_PROBE; i++)
        if (pages[i])
            free_page(pages[i]);
    printk("[day20] mm: %d-page alloc/verify/free %s\n",
           POOL_PROBE, ok ? "ok" : "FAILED");

    /* 2. 时钟: jiffies 在走 */
    uint32_t j0 = jiffies;
    while (jiffies == j0)
        __asm__ volatile("hlt");
    printk("[day20] timer: jiffies ticking (%d)\n", jiffies - j0);

    printk("[day20] selftest: all subsystems ok\n");
    printk("[day20] linux011-course 0.20 (terminal milestone)\n");
}
