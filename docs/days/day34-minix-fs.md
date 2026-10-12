# day34 - Minix v1 文件系统:超级块/位图/inode/buffer cache

ramfs(day15)退役。今天课程内核挂载**真文件系统**: 第二块 IDE 盘上是
一张宿主端 `tools/mkminix.py` 制作的 Minix v1 镜像(魔数 0x137F,
1KB/块), 内核侧新增 buffer cache + inode/bmap/namei 三层,
全部 I/O 走 day33 的 request 队列。0.11 的 `fs/*` +
`fs/buffer.c` 至此在课程内核里各就各位。

## 已实现并验证(test 19/19)

- `tools/mkminix.py`(宿主): 引导块|超级块|imap|zmap|inode 表|数据区;
  32B minix inode(u16 mode/uid, u32 size/mtime, u8 gid/nlinks,
  u16 zone[9]); 根目录=1 号 inode, 普通文件从 2 号起分配;
  预装 init/hello/welcome.txt/motd
- `kernel/buffer.c`: 8×1KB buffer, hash 桶 + 空闲链,
  bread(命中/缺失)/bwrite(同步回写)/brelse; 逐出脏块先回写;
  bhit/bmiss 统计供自检
- `kernel/minix.c`: minix_mount(校验魔数)/iget(inode 表→内存
  inode)/bmap(7 直接 + 1 一级间接, 与 0.11 相同的三段式前两段)/
  fs_lookup(根目录 24B 目录项线性查找)/fs_read(按块循环)/fs_ls
- 全内核换线: execve 用 fs_size; do_no_page 的取页路径改
  fs_read(经 buffer cache + 块层); shell 的 ls/cat 走 fs;
  init 的预装也从 fs 读
- 自检新增: buffer cache 命中/回写/逐出验证(写 block 400 →
  用 10 个新块把 8 个 buffer 转一圈逐出 → 重读校验)

启动串口证据链: `minix v1 mounted` → init 预装(读 inode 表块
4 + 数据块 6) → `do_no_page: pid 2 off 0 <- hello`(经
fs_read+buffer cache+ll_rw_blk 取来代码页) → hello 全流程照旧绿。

## 调试实录: 一次"数据闪烁"的层层排错(本课最值钱)

症状: init 能从 fs 装载(目录第 0 项), execve("hello") 却 lookup 失败;
探针显示目录 buffer"有时有 hello 有时全零"。

1. 先怀疑**双重读取竞态/中断上下文写坏内存** —— 关中断复测, 不变;
2. 再怀疑 **TLB 陈旧映射** —— bread 后整卷刷 CR3, 不变;
3. 让字面量偏移与运行时偏移**同一行对照读**, 才发现: `v32` 读到
   `0x00030000` —— 字节排布是 `[00 00 03 00]`: **数据没错, 是错位**
   —— 第 1 项的 ino 落在 +26 而不是 +24。
4. 真凶: `mkminix.py` 的目录项打包用了 `'<H21sxxx'` = **26 字节**,
   内核按 Minix 目录项 24 字节步长读。第 0 项恰好在偏移 0 处对齐,
   所以 init 蒙对了; 第 1 项起全部错位 2 字节。
5. 修复: `'<H21sx'`(2+21+1=24)。

教训三连: **(a)** 双方约定结构布局时, 用 `struct.calcsize` 当测试断言;
**(b)** "数据时有时无"先查偏移错位, 别急着怀疑竞态/TLB;
**(c)** 探针要设计成能让两种假设**同场对质**(同一行打印两种读法)。

## 对照阅读

- `source/include/linux/minix_fs_sb.h`: minix_super_block 与
  minix_inode 的逐字段定义
- `source/fs/super.c`: minix_read_super —— 挂载流程
- `source/fs/bitmap.c`: inode/zone 位图分配(本课位图只读解析,
  分配在 day35 的写路径接上)
- `source/fs/inode.c` + `namei.c`: iget/bmap/namei 全家
- `source/fs/buffer.c`: 940 buffer 双链 + hash, bread/brelse/bwrite

## 课程简化

- 单层根目录(0.11 的多级路径 namei 留作扩展); 位图只读; 文件只读
  (写路径 = day35 的 open/write/lseek + ialloc/zone 分配)
- buffer 仅 8 个、无睡眠等待、bwrite 同步落盘(0.11 是后台回写+睡眠)
