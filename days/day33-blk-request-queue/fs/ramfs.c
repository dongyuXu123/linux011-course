/* =====================================================================
 * day15 - fs/ramfs.c：内存文件系统(0.11 fs 的思想模型)
 * ---------------------------------------------------------------------
 * 结构对齐 source/include/linux/fs.h 的概念: 超级块(总量/位图)、
 * inode(类型/大小/数据块)、目录项。真 0.11 是 Minix 文件系统+
 * 块设备缓冲; 课程内核先把同一套抽象放进内存, day18 落到盘。
 * ===================================================================== */
#include <kernel.h>
#include <string.h>

#define RAMFS_MAX_FILES 16
#define RAMFS_NAMELEN   16
#define RAMFS_FILEMAX   4096

struct ramfs_inode {
    int         used;
    int         is_dir;
    uint32_t    size;
    char        name[RAMFS_NAMELEN];
    char        data[RAMFS_FILEMAX];
};

static struct ramfs_inode inodes[RAMFS_MAX_FILES];
static int ramfs_ready = 0;

void ramfs_init(void)
{
    memset(inodes, 0, sizeof(inodes));
    ramfs_ready = 1;
    printk("[day15] ramfs mounted: %d inodes, %d bytes/file cap\n",
           RAMFS_MAX_FILES, RAMFS_FILEMAX);
}

static struct ramfs_inode *ramfs_lookup(const char *name)
{
    for (int i = 0; i < RAMFS_MAX_FILES; i++)
        if (inodes[i].used && !strcmp(inodes[i].name, name))
            return &inodes[i];
    return 0;
}

int ramfs_create(const char *name)
{
    if (!ramfs_ready || ramfs_lookup(name))
        return -1;
    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (!inodes[i].used) {
            inodes[i].used = 1;
            inodes[i].is_dir = 0;
            inodes[i].size = 0;
            strncpy(inodes[i].name, name, RAMFS_NAMELEN - 1);
            return i;
        }
    }
    return -1;
}

int ramfs_write(const char *name, const char *buf, uint32_t len)
{
    struct ramfs_inode *f = ramfs_lookup(name);
    if (!f || len > RAMFS_FILEMAX)
        return -1;
    memcpy(f->data, buf, len);
    f->size = len;
    return len;
}

int ramfs_read(const char *name, char *buf, uint32_t len)
{
    struct ramfs_inode *f = ramfs_lookup(name);
    if (!f)
        return -1;
    if (len > f->size)
        len = f->size;
    memcpy(buf, f->data, len);
    return len;
}

/* day32 按需调页的取页路径(0.11 do_no_page 里 bmap+ll_rw_blk 读单页
   的对应物): 从文件偏移 off 处读一段 —— 越过文件尾的部分补零 */
int ramfs_read_at(const char *name, uint32_t off, char *buf, uint32_t len)
{
    struct ramfs_inode *f = ramfs_lookup(name);
    if (!f)
        return -1;
    uint32_t n = 0;
    if (off < f->size) {
        n = f->size - off;
        if (n > len)
            n = len;
        memcpy(buf, f->data + off, n);
    }
    if (n < len)
        memset(buf + n, 0, len - n);   /* 文件尾之后 = 零(bss) */
    return (int)len;
}

int ramfs_size(const char *name)
{
    struct ramfs_inode *f = ramfs_lookup(name);
    return f ? (int)f->size : -1;
}

/* ls: 列目录(内核侧直接打印; day16 起走 read API) */
void ramfs_ls(void)
{
    int n = 0;
    printk("name             size\n");
    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (!inodes[i].used)
            continue;
        printk("%-16s %d\n", inodes[i].name, inodes[i].size);
        n++;
    }
    printk("%d file(s), %d free inode(s)\n", n, RAMFS_MAX_FILES - n);
}
