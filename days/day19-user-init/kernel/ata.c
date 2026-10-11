/* =====================================================================
 * ata.c —— ATA PIO 轮询驱动(主通道主盘): 供块设备读取 initrd
 * 对照阅读: source/kernel/blk_drv/hd.c —— 0.11 的硬盘驱动走中断+
 *         request队列; 课程版用最简轮询, 概念一一对应(状态位/LBA/扇区)。
 * QEMU: -drive file=xx,if=ide → 主通道主盘 (0x1F0-0x1F7, 控制 0x3F6)
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

#define ATA_CMD_IDENTIFY 0xEC
#define ATA_CMD_READ_PIO 0x20

#define ST_BSY  0x80
#define ST_DRQ  0x08
#define ST_ERR  0x01

static inline void outb(uint16_t p, uint8_t v) { __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p)); }
static inline uint8_t inb(uint16_t p) { uint8_t r; __asm__ volatile("inb %1,%0":"=a"(r):"Nd"(p)); return r; }
static inline uint16_t inw(uint16_t p) { uint16_t r; __asm__ volatile("inw %1,%0":"=a"(r):"Nd"(p)); return r; }

static int ata_present = 0;
static uint8_t ata_drive = 0xA0;

static void ata_wait_bsy(void)
{
    while (inb(ATA_STATUS) & ST_BSY)
        ;
}

static int ata_identify(uint8_t drive)
{
    outb(ATA_DRIVE, drive);
    ata_wait_bsy();
    outb(ATA_SECCNT, 0);
    outb(ATA_LBA_LO, 0);
    outb(ATA_LBA_MID, 0);
    outb(ATA_LBA_HI, 0);
    outb(ATA_COMMAND, ATA_CMD_IDENTIFY);
    if (inb(ATA_STATUS) == 0)
        return 0;
    ata_wait_bsy();
    if (inb(ATA_STATUS) & ST_ERR)
        return 0;
    for (int i = 0; i < 256; i++)
        inw(ATA_DATA);
    return 1;
}

int ata_init(void)
{
    /* QEMU 里 initrd 盘可能落在主/从任一槽位, 依次探测 */
    if (ata_identify(0xA0)) { ata_present = 1; ata_drive = 0xA0; return 1; }
    if (ata_identify(0xB0)) { ata_present = 1; ata_drive = 0xB0; return 1; }
    return 0;
}



/* 读 n 个扇区到 buf(物理地址须在恒等映射内) */
int ata_read_sectors(uint32_t lba, int count, void *buf)
{
    if (!ata_present)
        return -1;
    uint16_t *w = (uint16_t *)buf;
    for (int s = 0; s < count; s++) {
        ata_wait_bsy();
        outb(ATA_DRIVE, 0xE0 | ata_drive | ((lba >> 24) & 0x0F));
        outb(ATA_SECCNT, 1);
        outb(ATA_LBA_LO, lba & 0xFF);
        outb(ATA_LBA_MID, (lba >> 8) & 0xFF);
        outb(ATA_LBA_HI, (lba >> 16) & 0xFF);
        outb(ATA_COMMAND, ATA_CMD_READ_PIO);
        uint8_t st;
        while (!((st = inb(ATA_STATUS)) & ST_DRQ)) {
            if (st & ST_ERR)
                return -1;
        }
        for (int i = 0; i < 256; i++)
            w[s * 256 + i] = inw(ATA_DATA);
        lba++;
    }
    return count;
}
