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

static uint32_t bmap_rw(struct minix_inode *ino, uint32_t blk, int alloc);
static uint32_t zalloc(void);
static uint16_t ialloc(void);

/* ---- inode.c: bmap —— 文件逻辑块 → 磁盘 zone(0.11 同名) ----
   0.11: 7 直接 + 1 一级间接(512 项) + 1 二级间接; 课程实现前两段 */
static uint32_t bmap(struct minix_inode *ino, uint32_t blk)
{
    return bmap_rw(ino, blk, 0);
}

/* day35: alloc=1 时按需分配缺的 zone/间接块(0.11 bmap 的写路径) */
static uint32_t bmap_rw(struct minix_inode *ino, uint32_t blk, int alloc)
{
    if (blk < 7) {
        if (ino->zone[blk] || !alloc)
            return ino->zone[blk];
        uint32_t z = zalloc();
        if (!z)
            return 0;
        ino->zone[blk] = z;
        return z;
    }
    blk -= 7;
    if (blk >= 512)
        return 0;                   /* 二级间接未用(course) */
    if (!ino->zone[7]) {
        if (!alloc)
            return 0;
        ino->zone[7] = zalloc();    /* 间接块本身 */
        if (!ino->zone[7])
            return 0;
    }
    struct buffer_head *bh = bread(fs_dev, ino->zone[7]);
    if (!bh)
        return 0;
    uint16_t z;
    memcpy(&z, bh->data + blk * 2, 2);
    if (!z && alloc) {
        z = zalloc();
        if (z)
            memcpy(bh->data + blk * 2, &z, 2);
    }
    if (z)
        bwrite(bh);                 /* 课程同步写: 位图/间接块即改即落 */
    brelse(bh);
    return z;
}

/* ---- bitmap.c: zone 分配(0.11 new_block 的课程版) ----
   位图常驻一个 buffer, 分配即置位即回写 —— 课程的同步元数据策略 */
static uint32_t zalloc(void)
{
    static struct buffer_head *zbh;
    if (!zbh) {
        zbh = bread(fs_dev, 2 + sb.imap_blocks);
        if (!zbh)
            return 0;
    }
    for (uint32_t z = sb.firstdatazone; z < sb.n_zones; z++) {
        if (!((zbh->data[z >> 3] >> (z & 7)) & 1)) {
            zbh->data[z >> 3] |= (char)(1 << (z & 7));
            bwrite(zbh);
            return z;
        }
    }
    return 0;
}

/* ---- bitmap.c: inode 分配(0.11 new_inode 课程版): 置位图+清盘上inode */
static uint16_t ialloc(void)
{
    static struct buffer_head *ibh;
    if (!ibh) {
        ibh = bread(fs_dev, 2);
        if (!ibh)
            return 0;
    }
    for (uint16_t ino = 2; ino <= sb.n_inodes; ino++) {
        if (!((ibh->data[ino >> 3] >> (ino & 7)) & 1)) {
            ibh->data[ino >> 3] |= (char)(1 << (ino & 7));
            bwrite(ibh);
            /* 盘上 inode 清零(inode 表块: 2+imap+zmap 起) */
            uint32_t tbl = 2 + sb.imap_blocks + sb.zmap_blocks
                         + (ino - 1) / 32;
            struct buffer_head *t = bread(fs_dev, tbl);
            if (t) {
                memset(t->data + ((ino - 1) % 32) * 32, 0, 32);
                bwrite(t);
                brelse(t);
            }
            return ino;
        }
    }
    return 0;
}

/* 内存 inode → 盘上 32B(minix_iupdate) */
static void inode_sync(struct minix_inode *n)
{
    uint32_t tbl = 2 + sb.imap_blocks + sb.zmap_blocks + (n->ino - 1) / 32;
    struct buffer_head *bh = bread(fs_dev, tbl);
    if (!bh)
        return;
    uint32_t off = ((n->ino - 1) % 32) * 32;
    memcpy(bh->data + off, &n->mode, 2);            /* u16 mode */
    memcpy(bh->data + off + 4, &n->size, 4);        /* u32 size */
    memcpy(bh->data + off + 14, n->zone, 18);       /* u16 zone[9] */
    bwrite(bh);
    brelse(bh);
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
    return fs_read_ino(inum, off, buf, len);
}

/* 按 inode 号读: fd 版 sys_read 走这里(与 fs_read 同一块循环) */
int fs_read_ino(int inum, uint32_t off, char *buf, uint32_t len)
{
    struct minix_inode *ino = iget(inum);
    if (!ino)
        return -1;
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
        uint32_t cnt = 1024 - inblk;
        if (cnt > len - done)
            cnt = len - done;
        if (cnt > ino->size - (off + done))
            cnt = ino->size - (off + done);
        memcpy(buf + done, bh->data + inblk, cnt);
        brelse(bh);
        done += cnt;
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

/* ---- day35: 建文件(0.11 sys_creat → open 的 O_CREAT 半边) ----
   根目录找 ino==0 的空目录项写入; inode 从 ialloc 来 */
int fs_creat(const char *name)
{
    if (!fs_ready || fs_lookup(name))
        return -1;                      /* 已存在 */
    struct minix_inode *root = iget(1);
    if (!root)
        return -1;
    uint32_t z = bmap(root, 0);
    struct buffer_head *bh = z ? bread(fs_dev, z) : 0;
    int ino = 0;
    if (bh) {
        uint32_t nent = root->size / 24;
        if (nent > 1024 / 24)
            nent = 1024 / 24;
        for (uint32_t e = 0; e < nent && !ino; e++) {
            uint32_t doff = e * 24;
            uint16_t cur;
            memcpy(&cur, bh->data + doff, 2);
            if (cur == 0) {             /* 空槽: 就地占用 */
                ino = ialloc();
                if (ino) {
                    memcpy(bh->data + doff, &ino, 2);
                    memset(bh->data + doff + 2, 0, 21);
                    for (int k = 0; k < 21 && name[k]; k++)
                        bh->data[doff + 2 + k] = name[k];
                    bwrite(bh);
                }
            }
        }
        brelse(bh);
    }
    iput(root);
    if (!ino) {
        ino = ialloc();                 /* 目录无空槽: 课程从简, 丢文件 */
        if (ino)
            printk("[day35] fs_creat: no dir slot, %s lost\n", name);
    }
    return ino ? ino : -1;
}

/* ---- day35: 文件写(0.11 file_write 的块循环课程版) ---- */
int fs_write_ino(int inum, uint32_t off, const char *buf, uint32_t len)
{
    if (!fs_ready)
        return -1;
    struct minix_inode *n = iget(inum);
    if (!n)
        return -1;
    uint32_t done = 0;
    while (done < len) {
        uint32_t fblk = (off + done) / 1024;
        uint32_t z = bmap_rw(n, fblk, 1);
        if (!z)
            break;
        struct buffer_head *bh = bread(fs_dev, z);
        if (!bh)
            break;
        uint32_t inblk = (off + done) % 1024;
        uint32_t cnt = 1024 - inblk;
        if (cnt > len - done)
            cnt = len - done;
        memcpy(bh->data + inblk, buf + done, cnt);
        bwrite(bh);
        brelse(bh);
        done += cnt;
    }
    if (off + done > n->size)
        n->size = off + done;
    inode_sync(n);                      /* size/zone 回写: 持久化的关键 */
    iput(n);
    return (int)done;
}
