# day33 - 中断驱动块设备 + request 队列(0.11 ll_rw_blk)

day18 读 initrd 用的是 PIO 轮询: CPU 盯着状态寄存器干等。真 0.11 的
块 I/O 从来不这样 —— `blk_drv/ll_rw_blk.c` 维护一条 request 队列,
`blk_drv/hd.c` 把命令编程进硬盘控制器就撒手, 数据由硬盘中断搬运,
`end_request` 收尾并接力下一笔。今天把这套结构装进课程内核: initrd
读取和自检的写读回全部改走队列 + IRQ14。

## 已实现并验证(test 19/19)

- `kernel/blk.c`(新): 8 槽 request 队列, 状态机
  free→queued→inflight→done/failed; `ll_rw_blk()` 同步语义(入队后
  hlt 等完成, 0.11 sleep_on 的课程版); `blk_end_request()` 中断里
  收尾并 `do_request()` 接力(0.11 同名)
- `kernel/ata.c` 重写: `ata_start_req()` 只编程控制器(0.11
  do_hd_request 对应); IRQ14 处理器读路径一次搬完 nsect 扇区
  (read_intr 对应); IDENTIFY 等控制命令的杂散中断因 pending==0 被忽略
- `ata_init` 放开从片 PIC bit6(IRQ14) —— 0.11 request_irq(HD_IRQ) 的对应
- `main.c` initrd 读取改走 `ll_rw_blk(BLK_READ, 0, 128, ...)`
- 自检新增: 写 sector 3 → 读回 → 逐字节校验

启动序列串口可证: `blk: request queue ready` → `ata: IRQ14 armed` →
`initrd read via request queue + IRQ14` → 自检 `write/read-back
via request queue ok` → 整条 init/fork/execve/hello 流程照旧全绿。

## 调试实录(两个教科书级并发问题)

**1. QEMU 对 PIO write 的 DRQ 就绪不投递 INTRQ**(读会投)。
写请求因此永远等不到中断。0.11 `write_intr` 在真硬件上等的正是这个
中断; 课程驱动对写路径采用"提交侧短轮询写数据 + 同步收尾",
读路径保持全中断驱动 —— 讲义对照 0.11 read_intr/write_intr 双桩结构。

**2. 写完成中断与提交侧收尾竞争**: 数据刚写完、`pending` 还没清零时,
完成中断到达 → IRQ 处理器看到 pending≠0 且状态无 DRQ → 把请求错标
FAILED(state=4 与 state=3 随机出现)。修法: 写数据阶段关中断一口气
完成(pushfl/popfl 保护, 兼容从 do_request 中断链里调用的情形)。
教训: *驱动里"设备中断"与"提交路径"共享的一切状态, 都要定义清楚
归谁、在什么中断级别下改*。

## 对照阅读

- `source/blk_drv/ll_rw_blk.c`: make_request/add_request/do_request/
  end_request —— 队列状态机与电梯调度(课程用 FIFO, 电梯留作思考题)
- `source/blk_drv/hd.c`: do_hd_request(编程控制器) /
  read_intr / write_intr(中断里搬数据) / bad_rw_intr(重试)
- `kernel/blk_drv/blk.h`: REQUEST 结构体 —— 我们的 blk_request 就是它
