# day15 - ramfs：超级块/inode 模型的内存文件系统

进入文件系统周。`fs/ramfs.c` 用 0.11 fs 的概念模型搭一个内存文件系统：16 个 inode 槽（`struct ramfs_inode`：used/is_dir/size/name/data），文件名 16 字节、内容上限 4KB；提供 `ramfs_create/write/read/size/ls` 一组接口。`kmain` 预置 `welcome.txt`（39 字节）与 `motd`（22 字节）两个文件，shell 新增 `ls` 命令列出目录。真 0.11 是 Minix 文件系统 + 块设备缓冲，本课先把同一套抽象（超级块的容量位图、inode、目录项）放进内存，day18 让它落到盘。

测试注入 `ls⏎`。串口输出：

```
[day15] ramfs mounted: 16 inodes
[day15] linux011 mini shell
name             size
welcome.txt      39
motd             22
2 file(s), 14 free inode(s)
```

## 知识点

- **inode 槽位 = 简化的 inode 位图**：`inodes[16]` 数组的 `used` 字段就是"inode 使用位图"的退化形式。0.11 的 `fs/bitmap.c` 用 `imap`/`smap` 两张位图分别管 inode 与数据块，`new_inode()/free_inode()` 找第一个清零位——`ramfs_create` 的空槽扫描是同一算法。
- **超级块去哪了**：`RAMFS_MAX_FILES/NAMELEN/FILEMAX` 三个宏就是本文件系统的"超级块"（容量参数）。0.11 的 `struct super_block`（`fs/super.c`）存 `s_ninodes/s_nzones/s_imap_blocks...`，并在 `mount_root()` 时从设备第一块读入——内存版天然不需要反序列化。
- **目录即线性表**：`ramfs_ls` 遍历 inode 数组打印 `name/size`。0.11 的目录是"内容为 dirent 数组的文件"（`fs/namei.c` 的 `find_entry`），课程版把目录隐式化为 inode 表本身——对比着看能理解"目录也是一种文件"这个抽象省掉了什么。
- **数据块去哪了**：每 inode 内嵌 4KB `data[]`，等于"一块数据 + 无间接块"。0.11 的 Minix inode 有 7 个直接块 + 一二三级间接块（`fs/inode.c` 的 `bmap`）；课程版的单块模型让 read/write 都是 `memcpy`，先把 API 边界立起来。
- **read 的截断语义**：`ramfs_read` 里 `if (len > f->size) len = f->size`——读不能越过文件尾。0.11 `file_dev.c` 的 `file_read` 按 `min(剩余文件长度, 请求长度)` 逐块走缓冲，语义一致。
- **shell 命令挂接**：`ls` 作为内置命令直呼 `ramfs_ls()`（内核侧直接打印）；day16 起 `cat/write` 改走 read/write API——"内核命令直通"与"经 API 访问"两种风格并存是刻意安排的教学对照。

## 关键代码走读

（本课路径均相对 `days/day15-ramfs/`）

`fs/ramfs.c` 的数据模型：

```c
#define RAMFS_MAX_FILES 16
struct ramfs_inode {
    int         used;
    int         is_dir;
    uint32_t    size;
    char        name[RAMFS_NAMELEN];
    char        data[RAMFS_FILEMAX];   /* 单块存储, 4KB */
};
static struct ramfs_inode inodes[RAMFS_MAX_FILES];
```

`fs/ramfs.c` 的 `ramfs_create` —— 找空槽（new_inode 的退化版）：

```c
if (!ramfs_ready || ramfs_lookup(name)) return -1;   /* 重名拒绝 */
for (int i = 0; i < RAMFS_MAX_FILES; i++)
    if (!inodes[i].used) { inodes[i].used = 1; strncpy(...); return i; }
```

`fs/ramfs.c` 的 `ramfs_ls` —— 目录输出与统计：

```c
printk("name             size\n");
for (int i = 0; i < RAMFS_MAX_FILES; i++) {
    if (!inodes[i].used) continue;
    printk("%-16s %d\n", inodes[i].name, inodes[i].size);
    n++;
}
printk("%d file(s), %d free inode(s)\n", n, RAMFS_MAX_FILES - n);
```

## 验收

`test/expect.txt` 六行顺序断言：

1. `ramfs mounted: 16 inodes` —— 文件系统初始化完成；
2. `name             size` —— 注入的 `ls` 命令解析成功；
3. `welcome.txt      39` / `motd             22` —— 两个预置文件的**名字与字节数**精确匹配（`ramfs_write` 的长度记账正确）；
4. `2 file(s), 14 free inode(s)` —— 占用统计与位图一致。

无像素断言；`ls` 输出的对齐由 vsprintf 的 `%-16s` 保证（day09 的成果）。

## 对照阅读

- `source/fs/super.c` —— `mount_root()`：从根设备读超级块、校验 magic、装 `s_imap[]/s_zmap[]`、建根 inode。课程 `ramfs_init()` 的"挂载"就是把静态数组清零。
- `source/fs/bitmap.c` —— `new_inode/free_inode/new_block/free_block` 的位图操作，与本课的 `used` 标志对照。
- `source/include/linux/fs.h` —— `struct super_block`/`struct inode`/`struct dir_entry`：本课 `ramfs_inode` 的字段都能在里面找到原型（`i_size/i_zone/i_mode`）。
- `source/kernel/chr_drv/...` 无关本课；真正的块设备读取路径（软盘）0.11 在 `blk_drv/floppy.c`——day19 的 ATA 驱动与之对照。

## 实验

1. **加 rename 与删除**：实现 `ramfs_unlink`（`used=0`，注意 size/name 一起清）和 shell 的 `rm` 命令，再 `ls` 验证 free inode 计数回升——体验"回收"比"分配"更容易漏。
2. **突破单块限制**：把 `data[4096]` 改成 `uint32_t blocks[4]`（指向 day10 的 `get_free_page()` 页），实现最多 16KB 的文件——这正是从"玩具"走向"0.11 式多块 inode"的第一步，写完再回头看 `bmap` 会亲切很多。
