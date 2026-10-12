/* =====================================================================
 * day33 - kernel/blk.c：request 队列 —— 0.11 blk_drv/ll_rw_blk.c
 * ---------------------------------------------------------------------
 * 0.11 的块 I/O 都走同一条路: 上层(bread/write)构造 request 挂进
 * 队列, do_request() 让设备开跑, 设备中断里 end_request() 收尾并
 * 接力下一笔。队列的价值: 同步突发异步 —— 上层睡觉, 硬件干活。
 * 课程简化: FIFO 出队(0.11 是电梯排序, 见讲义); ll_rw_blk 同步
 * 语义, 等完成用 hlt 空转(0.11 sleep_on, day36 的 TTY 会真正睡)。
 * ===================================================================== */
#include <kernel.h>
#include <string.h>

#define NR_REQUEST 8

enum { R_FREE = 0, R_QUEUED, R_INFLIGHT, R_DONE, R_FAILED };

static struct blk_request request[NR_REQUEST];
static struct blk_request *cur;        /* 正在设备上的那笔 */
static int blk_ready = 0;

/* ata.c 提供: 编程 ATA 控制器启动传输, 完成时 IRQ14 → blk_end_request */
int ata_start_req(struct blk_request *r);

void blk_init(void)
{
    memset(request, 0, sizeof(request));
    cur = 0;
    blk_ready = 1;
    printk("[day33] blk: request queue ready (%d slots)\n", NR_REQUEST);
}

/* 队列出发: 取第一笔排队的交给设备(0.11 同名函数;
   0.11 在这里做电梯排序 merge, 课程保持 FIFO 留作思考题) */
static void do_request(void)
{
    if (!blk_ready || cur)
        return;
    for (int i = 0; i < NR_REQUEST; i++)
        if (request[i].state == R_QUEUED) {
            cur = &request[i];
            break;
        }
    if (!cur)
        return;
    cur->state = R_INFLIGHT;
    if (ata_start_req(cur) < 0) {      /* 设备不在: 立即失败, 继续接力 */
        cur->state = R_FAILED;
        cur = 0;
        do_request();
    }
}

/* 设备中断收尾(0.11 end_request): 唤醒等它的人, 放行下一笔 */
void blk_end_request(int ok)
{
    if (!cur)
        return;
    cur->state = ok ? R_DONE : R_FAILED;
    cur = 0;
    do_request();
}

/* 同步块 I/O: 入队 → 等完成。返回 0 成功, -1 失败 */
int ll_rw_blk(int cmd, uint32_t sector, int nsect, char *buf)
{
    if (!blk_ready || nsect <= 0 || nsect > 128)
        return -1;
    __asm__ volatile("cli");           /* 占槽: 不容中断插进来抢同一个槽 */
    struct blk_request *r = 0;
    for (int i = 0; i < NR_REQUEST; i++)
        if (request[i].state != R_QUEUED && request[i].state != R_INFLIGHT) {
            r = &request[i];
            break;
        }
    if (!r) {
        __asm__ volatile("sti");
        return -1;                     /* 队列满(0.11 会在这里睡觉等槽) */
    }
    r->cmd = cmd;
    r->sector = sector;
    r->nsect = nsect;
    r->buf = buf;
    r->state = R_QUEUED;
    __asm__ volatile("sti");
    do_request();
    while (r->state == R_QUEUED || r->state == R_INFLIGHT)
        __asm__ volatile("hlt");       /* 任何中断都会唤醒, 醒来再看状态 */
    return r->state == R_DONE ? 0 : -1;
}
