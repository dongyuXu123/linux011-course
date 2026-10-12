/* =====================================================================
 * day33 - kernel/ata.c：中断驱动 ATA 驱动(主通道) —— 0.11 blk_drv/hd.c
 * ---------------------------------------------------------------------
 * day18 的轮询版换成本课的中断版: ata_start_req 只负责"编程控制器",
 * 数据搬运挪进 IRQ14 处理器, 完成后 end_request 接力下一笔 ——
 * 与 0.11 hd.c 的 do_hd_request/read_intr/write_intr 结构一一对应。
 * QEMU: -drive file=xx,if=ide → 主通道 (0x1F0-0x1F7, 控制 0x3F6)
 * ===================================================================== */
#include <kernel.h>

#define ATA_DATA      0x1F0
#define ATA_ERROR     0x1F1
#define ATA_SECCNT    0x1F2
#define ATA_LBA_LO    0x1F3
#define ATA_LBA_MID   0x1F4
#define ATA_LBA_HI    0x1F5
#define ATA_DRIVE     0x1F6
#define ATA_STATUS    0x1F7
#define ATA_COMMAND   0x1F7
#define ATA_CONTROL   0x3F6

#define ATA_CMD_IDENTIFY  0xEC
#define ATA_CMD_READ_PIO  0x20
#define ATA_CMD_WRITE_PIO 0x30

#define ST_BSY  0x80
#define ST_DRQ  0x08
#define ST_ERR  0x01

static inline void outb(uint16_t p, uint8_t v) { __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p)); }
static inline uint8_t inb(uint16_t p) { uint8_t r; __asm__ volatile("inb %1,%0":"=a"(r):"Nd"(p)); return r; }
static inline uint16_t inw(uint16_t p) { uint16_t r; __asm__ volatile("inw %1,%0":"=a"(r):"Nd"(p)); return r; }
static inline void outw(uint16_t p, uint16_t v) { __asm__ volatile("outw %0,%1"::"a"(v),"Nd"(p)); }

static int ata_present = 0;
static uint8_t ata_drive = 0xA0;
static struct blk_request *pending;    /* 设备上正在跑的那笔(只一笔在飞) */

static void hd_interrupt(void *regs);

static int ata_wait_bsy(int guard)
{
    while ((inb(ATA_STATUS) & ST_BSY) && --guard)
        ;
    return guard;
}

/* 探测用控制命令: IDENTIFY 自身走轮询(驱动尚未就位, 谈不上中断) */
static int ata_identify(uint8_t drive)
{
    outb(ATA_DRIVE, drive);
    ata_wait_bsy(1000000);
    outb(ATA_SECCNT, 0);
    outb(ATA_LBA_LO, 0);
    outb(ATA_LBA_MID, 0);
    outb(ATA_LBA_HI, 0);
    outb(ATA_COMMAND, ATA_CMD_IDENTIFY);
    if (inb(ATA_STATUS) == 0)
        return 0;
    if (!ata_wait_bsy(1000000))
        return 0;
    if (inb(ATA_STATUS) & ST_ERR)
        return 0;
    for (int i = 0; i < 256; i++)
        inw(ATA_DATA);
    return 1;
}

int ata_init(void)
{
    if (ata_identify(0xA0))      { ata_present = 1; ata_drive = 0xA0; }
    else if (ata_identify(0xB0)) { ata_present = 1; ata_drive = 0xB0; }
    if (!ata_present) {
        printk("[day33] ata: no IDE disk\n");
        return 0;
    }
    /* 放开 IRQ14(从片 bit6) —— 0.11 hd.c 里 request_irq(HD_IRQ) 的对应 */
    outb(0xA1, inb(0xA1) & (uint8_t)~(1 << 6));
    register_irq(14, hd_interrupt);
    printk("[day33] ata: IDE disk ready, IRQ14 armed\n");
    return 1;
}

/* blk.c 的 do_request 调这里。
   READ: 编程控制器后立刻返回, 数据由 IRQ14 搬(0.11 read_intr)。
   WRITE: QEMU 不为 PIO write 的 DRQ 就绪投递 INTRQ(真硬件会, 0.11
   write_intr 正是等它)—— 课程驱动在提交侧短轮询写完数据, 设备落盘
   后本函数内直接收尾; 队列接力(end_request→do_request)不受影响。 */
int ata_start_req(struct blk_request *r)
{
    if (!ata_present)
        return -1;
    pending = r;
    uint32_t lba = r->sector;
    ata_wait_bsy(1000000);
    outb(ATA_CONTROL, 0);              /* nIEN=0: 允许中断 */
    outb(ATA_DRIVE, 0xE0 | ata_drive | ((lba >> 24) & 0x0F));
    outb(ATA_SECCNT, (uint8_t)r->nsect);
    outb(ATA_LBA_LO, lba & 0xFF);
    outb(ATA_LBA_MID, (lba >> 8) & 0xFF);
    outb(ATA_LBA_HI, (lba >> 16) & 0xFF);
    outb(ATA_COMMAND, r->cmd == BLK_WRITE ? ATA_CMD_WRITE_PIO
                                          : ATA_CMD_READ_PIO);

    if (r->cmd == BLK_WRITE) {
        /* 数据搬运+收尾必须关中断一口气完成: 写落盘的完成中断若插进
           来, IRQ 处理器会看到 pending≠0 且无 DRQ, 把请求错标 FAILED */
        uint32_t fl;
        __asm__ volatile("pushfl; popl %0; cli" : "=r"(fl));
        uint16_t *w = (uint16_t *)r->buf;
        for (int s = 0; s < r->nsect; s++) {
            if (!ata_wait_bsy(1000000)) {
                pending = 0;
                blk_end_request(0);
                __asm__ volatile("pushl %0; popfl" : : "r"(fl));
                return 0;
            }
            uint8_t st = inb(ATA_STATUS);
            if (!(st & ST_DRQ) || (st & ST_ERR)) {
                pending = 0;
                blk_end_request(0);
                __asm__ volatile("pushl %0; popfl" : : "r"(fl));
                return 0;
            }
            for (int i = 0; i < 256; i++)
                outw(ATA_DATA, w[s * 256 + i]);
        }
        /* 等设备把数据落到介质(真硬件这里也是 INTRQ) */
        ata_wait_bsy(10000000);
        pending = 0;
        blk_end_request(1);
        __asm__ volatile("pushl %0; popfl" : : "r"(fl));
    }
    return 0;
}

/* IRQ14: 读数据搬运 + 收尾(0.11 read_intr)。
   本中断里一次搬完 nsect 个扇区(扇区间只轮询 BSY/DRQ);
   完成后的多余中断因 pending==0 被忽略 */
static void hd_interrupt(void *regs)
{
    (void)regs;
    struct blk_request *r = pending;
    if (!r)
        return;                        /* IDENTIFY 等控制命令的杂散中断 */
    pending = 0;
    uint16_t *w = (uint16_t *)r->buf;
    for (int s = 0; s < r->nsect; s++) {
        if (!ata_wait_bsy(1000000)) {
            blk_end_request(0);
            return;
        }
        uint8_t st = inb(ATA_STATUS);
        if (!(st & ST_DRQ) || (st & ST_ERR)) {
            blk_end_request(0);
            return;
        }
        if (r->cmd == BLK_WRITE)
            for (int i = 0; i < 256; i++)
                outw(ATA_DATA, w[s * 256 + i]);
        else
            for (int i = 0; i < 256; i++)
                w[s * 256 + i] = inw(ATA_DATA);
    }
    blk_end_request(1);
}
