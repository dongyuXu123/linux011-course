# day35 - 持久化根文件系统 + 0.11 ABI 系统调用补全

day34 的 Minix fs 只能读。今天接上写路径并补全 0.11 的文件系统调用:
open=5 / close=6 / creat=8 / lseek=19 加盟 read/write, 文件经
`task->fds[]`(0.11 filp[] 课程版)统一编址。init 在 ring3 用这套
ABI 完成 creat→write→lseek→read-back 的持久化闭环。

## 已实现并验证(test 21/21)

- `minix.c` 写半边(0.11 fs/bitmap.c + inode.c 的写路径):
  - `zalloc`/`ialloc`: 位图扫描分配, 置位即 bwrite(课程同步元数据)
  - `bmap_rw(alloc=1)`: 文件长出时按需分配 zone/一级间接块
  - `fs_creat`: ialloc + 占用根目录 ino==0 空槽写目录项
  - `fs_write_ino`: 块循环写 + size 增长 + `inode_sync` 把
    mode/size/zone[9] 回写盘上 32B inode —— **持久化的关键一步**
- `task_struct.fds[8]`: {ino, off}; fd0=stdin(tty 行读),
  fd1=stdout(console), 文件 fd 从 2 起
- syscall 号沿用 0.11: open=5, close=6, creat=8, lseek=19;
  read/write 按分流到 tty / console / 文件
- init 新增 `save` 命令: creat("notes.txt") → write("persist me
  day35") → lseek(0) → read 16B 逐字节校验 → close,
  串口出 `[day35] readback after lseek: ok (write persisted)`

## 设计要点

**fd 分流**(0.11 的 file 表概念): read/write 不再写死控制台 ——
fd 决定后端。0/1 固定字符设备, ≥2 查 fds[].ino 走文件。offset 存在
fd 里而不是 inode 里 —— 这正是 0.11 file 结构与 inode 分离的原因。

**元数据同步策略**: 位图与目录项即改即落(bwrite), inode 在 write
收尾统一 sync。真 0.11 有完整的 buffer 后台回写与 sync 系统调用,
课程选择"全同步"换可预测性 —— 讲义里对照 0.11 的 buffer 睡眠队列。

**失败路径**: 目录无空槽时 fs_creat 放弃并打印(0.11 会扩展目录块,
课程留作思考题); 队列满/位图耗尽返回 -1 由调用方兜底。

## 对照阅读

- `source/fs/bitmap.c`: new_block/new_inode —— 位图扫描 + buffer 修改
- `source/fs/file_dev.c`: file_read/file_write —— fd 分发与偏移
- `source/fs/open.c`: sys_open/sys_creat/sys_close
- `source/fs/read_write.c`: sys_lseek 的 whence 三分支

## 课程简化

- 无 O_RDONLY/O_WRONLY 权限检查; lseek 不支持 SEEK_END
- 目录不扩展(固定槽位); 无 unlink —— 回收位图留作练习
