# day40 - 收官:0.11 内核子集 + Haribote 式桌面

四十天课程的终点。回望 day01 的 512 字节 MBR, 今天这台机器已经有了:

**引导与内存**: GRUB multiboot → 2MB 内核 → 分页开启 → 每任务独立
64MB 线性空间(LDT 代码/数据段 DPL3 + 私有页目录 + CR3/LDTR 随任务
切换), 按需调页(execve 只登记文件, 缺页才从盘读, bss 给零页)。

**进程与系统调用**: 软件上下文切换, fork(copy_mem 逐页复制)/
execve(start_thread 式改写 iret 现场)/exit/wait4; 0.11 调用号
exit=1 fork=2 read=3 write=4 open=5 close=6 creat=8 time=13
lseek=19 getpid=20 wait4=72, 外加课程窗口 API win_create=41/
win_write=42/win_focus=43。用户指针经 UPTR 翻译, 内核缓冲纪律
防 CR3 抢占。

**块设备与文件系统**: request 队列(8 槽, end_request 中断收尾接力),
ATA 主通道 IRQ14 读驱动; Minix v1 文件系统 —— 超级块(0x137F)/
inode/zone 位图/bmap(7 直接+1 一级间接)/buffer cache(8×1KB hash+LRU
逐出回写); 持久化写路径(zalloc/ialloc/inode_sync)。

**TTY**: 行规程(退格/^U/回显/整行交付) + 双虚拟控制台。

**图形**: VBE 640×480×32(LFB 经 PCI BAR 实测), 双缓冲图层合成器,
PS/2 鼠标点击置顶/标题条拖动, 任务栏, 终端窗, 用户态图形应用
(clock/files/gdemo 经窗口 API), cat/w 落 Minix fs, 焦点轮转。

**验收**: `make test-all` —— 40 课全部串口顺序断言 + 图形课像素
断言通过; 每课快照独立可复现(`make -C days/dayNN test`)。

## 阶段二的"向 0.11 靠拢"清单

| 子系统 | 0.11 原始设计 | 课程落点 |
|---|---|---|
| 内存 | copy_mem/page.s/do_no_page | day31/32 |
| 块设备 | ll_rw_blk.c + hd.c | day33 |
| 文件系统 | super/bitmap/inode/namei/buffer | day34/35 |
| 系统调用 | sys.c 调用号与 fd 语义 | day35 |
| TTY | tty_io.c read_chan 规范模式 | day36 |
| 图形 | (0.11 无) | day21-30, 37-39 为课程扩展 |

## 若要继续生长

- buffer cache 后台回写 + sync; 目录读系统调用(readdir); unlink
- 电梯调度替换 FIFO request 队列; 多进程同睡队列
- COW fork(0.11 是全复制, 现代内核思路的对照实验)
- 窗口 API 补销毁/移动/事件; 应用走 elf 装载(现为平铺 aout)
