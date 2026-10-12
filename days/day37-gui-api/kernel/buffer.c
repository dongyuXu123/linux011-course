/* =====================================================================
 * day34 - kernel/buffer.c：缓冲区缓存 —— 0.11 fs/buffer.c
 * ---------------------------------------------------------------------
 * 0.11 用 940 个 1KB buffer 组成 双链(空闲 LRU + hash 链), bread 命中
 * 直接回内存, 未命中经 ll_rw_blk 从设备读入; bwrite 反向落盘。
 * 块层(day33)由此获得第二类用户 —— 文件系统。
 * 课程简化: 8 个 buffer, hash 桶固定 4, 无睡眠等待(队列槽够用)。
 * ===================================================================== */
#include <kernel.h>
#include <string.h>

#define NR_BUF      8
#define NR_HASH     4

enum { B_FREE = 0, B_VALID, B_DIRTY };

static struct buffer_head buffers[NR_BUF];
static struct buffer_head *hash[NR_HASH];
static struct buffer_head *freelist;
static uint32_t bhit, bmiss;    /* 自检可见的命中率统计 */

void buffer_init(void)
{
    memset(buffers, 0, sizeof(buffers));
    memset(hash, 0, sizeof(hash));
    freelist = 0;
    for (int i = 0; i < NR_BUF; i++) {   /* 尾插 → 先分配 0 号(0.11 相反,
                                            这里为了测试输出可预测) */
        buffers[i].fnext = freelist;
        freelist = &buffers[i];
    }
    printk("[day34] buffer cache: %d x 1KB buffers ready\n", NR_BUF);
}

static struct buffer_head **hash_slot(uint32_t dev, uint32_t block)
{
    return &hash[(block + dev * 7) % NR_HASH];
}

/* getblk: 找(或腾)一个 buffer 挂上 dev/block —— 不含数据搬运 */
static struct buffer_head *getblk(uint32_t dev, uint32_t block)
{
    for (struct buffer_head *h = *hash_slot(dev, block); h; h = h->hnext)
        if (h->dev == dev && h->block == block)
            return h;                    /* 已缓存 */
    /* 摘一个空闲 buffer(非 dirty 优先; 课程简化: dirty 的先回写) */
    struct buffer_head *b = 0;
    for (int pass = 0; pass < 2 && !b; pass++)
        for (struct buffer_head *f = freelist; f; f = f->fnext)
            if (f->count == 0 &&
                (pass == 1 || f->state != B_DIRTY)) {
                b = f;
                break;
            }
    if (!b)
        return 0;                        /* 全忙: 课程里不会发生(8 槽) */
    if (b->state == B_DIRTY)             /* 逐出前回写(0.11 同样 here) */
        bwrite(b);
    if (b->hnext || *hash_slot(b->dev, b->block) == b) {
        struct buffer_head **p = hash_slot(b->dev, b->block);
        while (*p && *p != b)
            p = &(*p)->hnext;
        if (*p)
            *p = b->hnext;
    }
    b->dev = dev;
    b->block = block;
    b->state = B_FREE;
    b->count = 0;
    b->hnext = *hash_slot(dev, block);
    *hash_slot(dev, block) = b;
    return b;
}

/* bread: 命中直接返回; 未命中 ll_rw_blk 读入(0.11 ll_rw_block) */
struct buffer_head *bread(uint32_t dev, uint32_t block)
{
    struct buffer_head *b = getblk(dev, block);
    if (!b)
        return 0;
    b->count++;
    if (b->state == B_VALID) {
        bhit++;
        return b;
    }
    bmiss++;
    if (ll_rw_blk(BLK_READ, block * 2, 2, b->data) < 0) {
        b->count--;
        return 0;
    }
    b->state = B_VALID;
    return b;
}

/* bwrite: 标脏并立即落盘(0.11 是后台回写; 课程简化为同步) */
void bwrite(struct buffer_head *b)
{
    if (ll_rw_blk(BLK_WRITE, b->block * 2, 2, b->data) < 0) {
        b->count--;
        return;
    }
    b->state = B_VALID;
}

void brelse(struct buffer_head *b)
{
    if (b && b->count > 0)
        b->count--;
}

uint32_t buffer_stats(uint32_t *miss)
{
    if (miss)
        *miss = bmiss;
    return bhit;
}
