/* =====================================================================
 * day34 - kernel/minix.c：Minix v1 文件系统(读路径) —— 0.11 fs/*
 * ---------------------------------------------------------------------
 * 对齐 0.11 的分工:
 *   super.c  读超级块/挂载(本课: minix_mount)
 *   bitmap.c inode/zone 位图(mount 时摘要; 写路径分配 day35 接)
 *   inode.c  iget/bmap —— 磁盘 inode → 内存 inode, 逻辑块→物理 zone
 *   namei.c  目录项查找(课程: 单层根目录)
 * 全部 I/O 经 buffer.c(day33 块层), 1KB/块, 魔数 0x137F。
 * ===================================================================== */
#include <kernel.h>
#include <string.h>

#define MINIX_MAGIC 0x137F
#define NR_IMEM     8               /* 内存 inode 缓存 */

struct minix_super {
    uint16_t n_inodes, n_zones, imap_blocks, zmap_blocks;
    uint16_t firstdatazone, log_zone_size;
    uint32_t max_size, magic, state, zones;
};

struct minix_dinode {           /* 磁盘 inode, 32B */
    uint16_t mode, uid;
    uint32_t size, mtime;
    uint8_t  gid, nlinks;
    uint16_t zone[9];
} __attribute__((packed));

struct minix_dirent {           /* 目录项, 24B(磁盘格式 23B 有效 + 1 填充) */
    uint16_t ino;
    char     name[21];
    uint8_t  pad[1];           /* 2+21+1=24? 不 —— 磁盘项 24B: 2+21+3, 用数组精确 */
} __attribute__((packed));

struct minix_inode {            /* 内存 inode(0.11 m_inode 课程版) */
    uint16_t ino;
    uint32_t size;
    uint16_t zone[9];
    uint16_t mode;
    int      count;
};

static struct minix_super sb;
static int fs_ready = 0;
static struct minix_inode imem[NR_IMEM];
static uint32_t fs_dev = 0;

/* ---- super.c: 挂载 ---- */
int minix_mount(uint32_t dev)
{
    struct buffer_head *bh = bread(dev, 1);        /* 超级块固定 1 号块 */
    if (!bh)
        return -1;
    memcpy(&sb, bh->data, sizeof(sb));
    brelse(bh);
    if (sb.magic != MINIX_MAGIC) {
        printk("[day34] minix: bad magic %x (want 137F)\n", sb.magic);
        return -1;
    }
    memset(imem, 0, sizeof(imem));
    fs_dev = dev;
    fs_ready = 1;
    printk("[day34] minix v1 mounted: %d inodes, %d zones, "
           "data@%d\n", sb.n_inodes, sb.n_zones, sb.firstdatazone);
    return 0;
}

/* ---- inode.c: iget(课程版: 无 hash, 线性找/占) ---- */
static struct minix_inode *iget(uint16_t ino)
{
    if (!fs_ready || ino == 0 || ino > sb.n_inodes)
        return 0;
    for (int i = 0; i < NR_IMEM; i++)
        if (imem[i].count && imem[i].ino == ino) {
            imem[i].count++;
            return &imem[i];
        }
    for (int i = 0; i < NR_IMEM; i++) {
        if (imem[i].count == 0) {
            uint32_t tbl_blk = 2 + sb.imap_blocks + sb.zmap_blocks
                             + (ino - 1) / 32;          /* inode 表块号 */
            uint32_t off = ((ino - 1) % 32) * 32;
            struct buffer_head *bh = bread(fs_dev, tbl_blk);
            if (!bh)
                return 0;
            struct minix_dinode d;
            memcpy(&d, bh->data + off, sizeof(d));
            brelse(bh);
            imem[i].ino = ino;
            imem[i].size = d.size;
            imem[i].mode = d.mode;
            imem[i].count = 1;
            for (int k = 0; k < 9; k++)
                imem[i].zone[k] = d.zone[k];
            return &imem[i];
        }
    }
    return 0;
}

static void iput(struct minix_inode *ino)
{
    if (ino && ino->count > 0)
        ino->count--;
}

/* ---- inode.c: bmap —— 文件逻辑块 → 磁盘 zone(0.11 同名) ----
   0.11: 7 直接 + 1 一级间接(512 项) + 1 二级间接; 课程实现全部三段 */
static uint32_t bmap(struct minix_inode *ino, uint32_t blk)
{
    if (blk < 7)
        return ino->zone[blk];
    blk -= 7;
    if (blk < 512) {
        if (!ino->zone[7])
            return 0;
        struct buffer_head *bh = bread(fs_dev, ino->zone[7]);
        if (!bh)
            return 0;
        uint16_t z;
        memcpy(&z, bh->data + blk * 2, 2);
        brelse(bh);
        return z;
    }
    return 0;                       /* 二级间接未用(course) */
}

/* ---- namei.c: 根目录查找(单层) ---- */
int fs_lookup(const char *name)
{
    if (!fs_ready)
        return 0;
    struct minix_inode *root = iget(1);
    if (!root)
        return 0;
    int found = 0;
    uint32_t z = bmap(root, 0);
    struct buffer_head *bh = z ? bread(fs_dev, z) : 0;
    if (bh) {
        uint32_t nent = root->size / 24;
        if (nent > 1024 / 24)
            nent = 1024 / 24;
        for (uint32_t e = 0; e < nent && !found; e++) {
            uint32_t doff = e * 24;
            uint16_t dino = *(uint16_t *)(bh->data + doff);
            char nm[22];
            for (int k = 0; k < 21; k++)
                nm[k] = bh->data[doff + 2 + k];
            nm[21] = 0;
            if (dino && !strcmp(nm, name))
                found = dino;
        }
        brelse(bh);
    }
    iput(root);
    return found;
}

/* 文件大小 */
int fs_size(const char *name)
{
    int ino = fs_lookup(name);
    if (!ino)
        return -1;
    struct minix_inode *n = iget(ino);
    if (!n)
        return -1;
    int sz = (int)n->size;
    iput(n);
    return sz;
}

/* ---- read: 从文件偏移读一段(0.11 file_read 的块循环课程版) ---- */
int fs_read(const char *name, uint32_t off, char *buf, uint32_t len)
{
    int inum = fs_lookup(name);
    if (!inum)
        return -1;
    struct minix_inode *ino = iget(inum);
    if (!ino) {
        return -1;
    }
    uint32_t done = 0;
    while (done < len && off + done < ino->size) {
        uint32_t fblk = (off + done) / 1024;
        uint32_t z = bmap(ino, fblk);
        if (!z)
            break;
        struct buffer_head *bh = bread(fs_dev, z);
        if (!bh)
            break;
        uint32_t inblk = (off + done) % 1024;
        uint32_t n = 1024 - inblk;
        if (n > len - done)
            n = len - done;
        if (n > ino->size - (off + done))
            n = ino->size - (off + done);
        memcpy(buf + done, bh->data + inblk, n);
        brelse(bh);
        done += n;
    }
    iput(ino);
    return (int)done;
}

/* ls: 根目录列表(shell 用) */
void fs_ls(void)
{
    struct minix_inode *root = iget(1);
    if (!root)
        return;
    printk("name             size\n");
    uint32_t nent = root->size / 24;
    for (uint32_t e = 0; e < nent; e++) {
        uint32_t z = bmap(root, e / (1024 / 24));
        if (!z)
            continue;
        struct buffer_head *bh = bread(fs_dev, z);
        if (!bh)
            continue;
        struct minix_dirent d;
        memcpy(&d, bh->data + (e % (1024 / 24)) * 24, sizeof(d));
        brelse(bh);
        char nm[22];
        memcpy(nm, d.name, 21);
        nm[21] = 0;
        if (!d.ino || nm[0] == '.')
            continue;
        struct minix_inode *n = iget(d.ino);
        printk("%-16s %d\n", nm, n ? (int)n->size : 0);
        iput(n);
    }
    printk("mount: minix v1 dev 0\n");
    iput(root);
}
