# day07 - timer-pit：8253/8259 到位，内核有了心跳

中断子系统上线。本课做三件事：把 8259A PIC 重映射到向量 `0x20~0x2F`（默认 `0x08~0x0F` 会与 CPU 异常撞车）、搭好 16 路 IRQ 统一入口与 C 分发表；编程 8253 PIT 通道 0 产生 **100Hz** 方波接在 IRQ0 上；开中断后用 `hlt` 空转等心跳——`jiffies` 每秒加 100，`timer_interrupt` 在第 100/200 个 tick 各打一行。day11 的调度器就从这一拍里长出来。

串口输出（测试要真等 3 秒，TIMEOUT 放到 45s 以上）：

```
[day07] enabling PIC remap + PIT (100Hz)
[day07] interrupts on, waiting 3 seconds...
[day07] tick 100 (1s heartbeat)
[day07] tick 200 (2s heartbeat)
[day07] jiffies=0x12c heartbeat ok
```

## 知识点

- **8259 重映射的必要性**：PC 兼容机上 8259 出厂占用向量 `0x08~0x0F`，与 CPU 异常 #DF(8)~#MF(16) 直接重叠——时钟一响就跳进"双故障"处理。ICW2 分别给主/从片指定新基址 `0x20/0x28`，IRQ0~7 → 0x20~0x27、IRQ8~15 → 0x28~0x2F。
- **ICW/OCW 编程时序**：ICW1（级联+要 ICW4）→ ICW2（向量基址）→ ICW3（主片 IR2 接从片）→ ICW4（8086 模式），主从两个端口组 `0x20/0x21` 与 `0xA0/0xA1` 对称写。之后 OCW1 屏蔽字：本课只放开 IRQ0（时钟）和 IRQ2（级联口），其余全关。
- **8253 PIT**：输入时钟 1.19318MHz，通道 0 接 IRQ0。写 `0x43=0x36`（通道 0、先低后高、方式 3 方波），再向 `0x40` 写 16 位除数 `LATCH = 1193180/100`——硬件每 1/100 秒触发一次 IRQ0。0.11 的 `LATCH` 常量与此完全相同。
- **EOI 规矩**：中断处理结束必须向 8259 发 EOI（OCW2 `0x20`）才允许下一个中断；来自从片（IRQ8~15）要**从片、主片各发一次**。忘了 EOI 的症状是"只进一次中断"，这是裸机调试头号坑。
- **`hlt` 与功耗/时序**：空等用 `hlt` 睡眠等中断唤醒，既是好习惯也让"jiffies 在走"与"CPU 在等"解耦。`sti()` 之后内核第一次有了"被动推进"的能力——之后所有阻塞（day08 的 `tty_getc`、day12 的 `wait`）都长在这个模式上。

## 关键代码走读

（本课路径均相对 `days/day07-timer-pit/`）

`kernel/irq.c` 的 `pic_init` —— 重映射核心四步（从片对称）：

```c
outb(0x20, 0x11);          /* ICW1: 边沿触发, 级联, ICW4 */
outb(0x21, 0x20);          /* ICW2: 主片向量基址 0x20 */
outb(0x21, 0x04);          /* ICW3: 主片 IR2 接从片 */
outb(0x21, 0x01);          /* ICW4: 8086 模式 */
outb(0x21, 0xFF & ~(1<<0) & ~(1<<2));  /* 只放开 IRQ0 与级联 IRQ2 */
```

`kernel/irq.c` 的 `do_irq` —— 按 IDT 向量反推 IRQ 并分发：

```c
void do_irq(struct pt_regs *regs)
{
    int irq = (int)regs->vector - 0x20;
    if (irq_handlers[irq])
        irq_handlers[irq]`(regs)`;
    irq_eoi(irq);              /* 从片要先发 0xA0 再发 0x20 */
}
```

`kernel/timer.c` 的 `timer_init`/`timer_interrupt` —— 0.11 timer_interrupt 的雏形：

```c
#define LATCH 1193180 / 100          /* 1.19318MHz / 100Hz */
register_irq(0, timer_interrupt);
outb(0x43, 0x36);              /* 通道0, 方式3(方波), 先低后高 */
outb(0x40, LATCH & 0xFF);
outb(0x40, (LATCH >> 8) & 0xFF);
/* timer_interrupt: jiffies++; 每 100/200 tick 打一行 */
```

`kernel/main.c` 的等待循环：`while (jiffies < 300) hlt;`——中断驱动的第一次"阻塞"。

## 验收

`test/expect.txt` 五行顺序断言：

1. `enabling PIC remap + PIT (100Hz)` —— 初始化路径开始；
2. `interrupts on, waiting 3 seconds...` —— `sti()` 已执行；
3. `tick 100 (1s heartbeat)` / `tick 200 (2s heartbeat)` —— 两行**间隔必须约 1 秒**且依次出现，证明频率确实是 100Hz（LATCH 写错、方式位错都会让拍子不准或不来）；测试框架按顺序消费日志，两条 tick 的先后即时间序；
4. `heartbeat ok` —— 300 tick 后主循环正常退出并走 `0xF4` 退出。

本课无像素断言。

## 对照阅读

- `source/kernel/sched.c` —— 0.11 把 `timer_interrupt` 直接写在调度器文件里：`jiffies++`、`current->counter--`、到点 `schedule()`。本课只做了前半（jiffies++），day11 的 `do_timer` 补上后半。
- `source/include/linux/sched.h` 的 `#define LATCH 1193180/HZ` 与 `init/main.c` 里 `outb_p(0x36, 0x43)` 的 PIT 初始化——常量与端口一模一样。
- 0.11 没有独立的 irq 分发文件：每个驱动在 `chr_drv/*.c` 里自己 `set_intr_gate` 并处理。课程的 `irq.c` 是把这套分散逻辑集中成 `register_irq` 表——与 Linux 2.x 之后 `request_irq()` 的形态对齐。

## 实验

1. **把节拍改成 1000Hz**：`LATCH` 除数换成 `1193180/1000`，`timer_interrupt` 的打印阈值乘 10（jiffies==1000 才是 1 秒）。验证后想想：tick 越快调度粒度越细，但每秒中断次数也越多——0.11 选 100Hz 的权衡是什么。
2. **忘掉 EOI 会怎样**：把 `irq_eoi(0)` 那行注释掉，预测并观察 `tick 100` 只能出现一次（8259 的 ISR 位不清理，同级中断全被挡住）——用一次"破坏性实验"记住这条铁律。
