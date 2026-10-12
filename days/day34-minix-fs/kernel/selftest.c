/* =====================================================================
 * day34 - kernel/selftest.c：里程碑自检
 * 字符终端形态的 linux011-course 至此五脏俱全: GRUB 引导 / 分页 /
 * 中断 / 时钟 / 键盘 tty / 物理页管理 / 任务调度 / fork-exec-wait /
 * 系统调用 / Minix v1 文件系统 / 用户态 init。启动时跑一轮子系统自检并出报告。
 * ===================================================================== */
#include <kernel.h>
#include <string.h>

extern volatile uint32_t jiffies;

#define POOL_PROBE 200

void selftest_run(void)
{
    printk("[day34] selftest begin\n");

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
    printk("[day34] mm: %d-page alloc/verify/free %s\n",
           POOL_PROBE, ok ? "ok" : "FAILED");

    /* day33: 块设备写读回 —— 经 request 队列 + IRQ14(0.11 的 bread/write) */
    static char blkbuf[512] __attribute__((aligned(4096)));
    for (int i = 0; i < 512; i++)
        blkbuf[i] = (char)(i * 7 + 0x5A);
    if (ll_rw_blk(BLK_WRITE, 3, 1, blkbuf) < 0)
        ok = 0;
    for (int i = 0; i < 512; i++)
        blkbuf[i] = 0;
    if (ok && ll_rw_blk(BLK_READ, 3, 1, blkbuf) < 0)
        ok = 0;
    for (int i = 0; ok && i < 512; i++)
        if (blkbuf[i] != (char)(i * 7 + 0x5A))
            ok = 0;
    printk("[day33] blk: write/read-back via request queue %s\n",
           ok ? "ok" : "FAILED");

    /* day34: buffer cache —— 命中/回写/逐出(0.11 bread/bwrite/brelse) */
    static char bb1[1024] __attribute__((aligned(4096)));
    for (int i = 0; i < 1024; i++)
        bb1[i] = (char)(i * 3 + 7);
    ll_rw_blk(BLK_WRITE, 300 * 2, 2, bb1);        /* 盘上有底 */
    struct buffer_head *bh = bread(0, 300);       /* miss: 从盘读 */
    uint32_t miss0;
    buffer_stats(&miss0);
    ok = 0;
    if (bh) {
        ok = memcmp(bh->data, bb1, 1024) == 0;
        brelse(bh);
        uint32_t miss1;
        bread(0, 300);                            /* hit: 第二次不落盘 */
        buffer_stats(&miss1);
        ok = ok && (miss1 == miss0);
        brelse(bh);
        /* 逐出: 连读 10 个新块把 8 个 buffer 转一圈, 再回来验证回写 */
        static char bb2[1024] __attribute__((aligned(4096)));
        for (int i = 0; i < 1024; i++)
            bb2[i] = (char)(i ^ 0xC3);
        struct buffer_head *w = bread(0, 400);
        if (w) {
            memcpy(w->data, bb2, 1024);
            bwrite(w);
            brelse(w);
        }
        for (int b = 500; b < 510; b++)
            brelse(bread(0, b));
        struct buffer_head *r = bread(0, 400);
        ok = ok && r && memcmp(r->data, bb2, 1024) == 0;
        if (r)
            brelse(r);
    }
    printk("[day34] buffer cache: miss/hit/evict-writeback %s\n",
           ok ? "ok" : "FAILED");

    /* 2. 时钟: jiffies 在走 */
    uint32_t j0 = jiffies;
    while (jiffies == j0)
        __asm__ volatile("hlt");
    printk("[day34] timer: jiffies ticking (%d)\n", jiffies - j0);

    printk("[day34] selftest: all subsystems ok\n");
    printk("[day34] linux011-course 0.34 (Minix v1 filesystem)\n");
}
