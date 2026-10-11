# day08 - keyboard-tty：键盘中断 → 环形缓冲 → tty 行规则

内核从"能说"进化到"能听"。IRQ1 键盘中断把扫描码从 `0x60` 端口读出、经简化的 set-1 键位表翻译成 ASCII，压入 64 字节的环形队列（tty `read_q`），同时回显到控制台；主线程 `tty_getc()` 阻塞读队列。整条链路"中断写队列、读取方睡 `hlt` 等中断"，与 0.11 的 tty 模型同构。`kmain` 读一行（处理退格），回车后把内容回显出来。

测试通过 QMP 向 guest 注入按键 `h,i,退格,i,回车`（见 `Makefile` 的 `KEYS := h,i,backspace,i,ret`），验证退格真的删掉了前一个字符。串口输出：

```
[day08] console ready, waiting for keystrokes
[day08] line='hi'
[day08] keyboard+tty ok
```

## 知识点

- **PS/2 键盘数据流**：按键产生 make code、松开产生 break code（set-1 规则：break = make | 0x80）。IRQ1 处理器从 8042 的数据口 `0x60` 取扫描码；`tty_init` 先向命令口 `0x64` 写 `0xAE` 启用第一 PS/2 接口。
- **shift 的状态机**：`0x2A/0x36`（左右 shift）的 make/break 维护 `shift_down` 标志，字母键输出时按它决定大小写。真实的 keymap 是"扫描码 × 修饰键"的多维表——0.11 `keyboard.S` 里有 8 张 `key_map*`，课程只保留一维可打印字符 + 退格 + 回车。
- **环形队列（tty_queue 思想）**：`head` 写指针、`tail` 读指针、`count` 计数，模 64 环绕。中断上下文只做 `q_put`（满了就丢——课程从简），用户上下文 `q_get`。这正是 0.11 `include/linux/tty.h` 里 `struct tty_queue { buf/head/tail/count }` 的最小实现。
- **回显位置**：回显发生在中断里（`con_putc(c)`），所以"打到一半"的字符立即可见；而行缓冲（攒到回车才算一行）由读取方完成。0.11 的 canonical 模式（`L_CANON`）也是这个分工。
- **阻塞的第二次实现**：`tty_getc` 在队列为空时 `hlt`——day07 的心跳模式第一次被用于等待"人类"。因为 `q_get` 返回 -1 表示空，循环条件天然处理"醒来还是空"的竞态。
- **键盘与定时器共用 irq 框架**：`tty_init` 只是 `register_irq(1, keyboard_interrupt)`——day07 的 16 路分发表让"接一个新中断源"降为两行代码，0.11 里这要重写一段 `keyboard.S` 入口并在 `traps.c` 挂门。

## 关键代码走读

（本课路径均相对 `days/day08-keyboard-tty/`）

`kernel/keyboard.c` 的 `keyboard_interrupt` —— 扫描码三分支：

```c
uint8_t sc = inb(0x60);
if (sc & 0x80) {                  /* break code: 只关心 shift 释放 */
    sc &= 0x7F;
    if (sc == 0x2A || sc == 0x36) shift_down = 0;
    return;
}
if (sc == 0x2A || sc == 0x36) { shift_down = 1; return; }  /* make */
char c = keymap[sc];
if (shift_down && c >= 'a' && c <= 'z') c -= 32;
q_put(&read_q, c);
con_putc(c);                      /* 回显 */
```

`kernel/keyboard.c` 的 `q_put` —— O(1) 环形写入：

```c
if (q->count >= QLEN) return;     /* 满了就丢 */
q->buf[q->head] = c;
q->head = (q->head + 1) % QLEN;
q->count++;
```

`kernel/keyboard.c` 的 `tty_getc` —— 阻塞读：

```c
while ((c = q_get(&read_q)) == -1)
    __asm__ volatile("hlt");      /* 睡到下一个中断 */
```

`kernel/main.c` 的行收集循环：`\b` 时 `n--`（回显的擦除已由 `con_putc` 做了）、`\n` 时收行报出 `line='...'`。缓冲 31 字符封顶（`if (n < 31)`），课程版不设行上限告警。

## 验收

`test/expect.txt` 三行顺序断言：

1. `console ready, waiting for keystrokes` —— tty 子系统初始化完成、中断已开；
2. ``line='hi'`` —— 注入序列 `h,i,backspace,i,ret` 的最终结果是 `hi`：这同时验证了 (a) 键位表翻译正确，(b) 退格既擦了屏幕也删了缓冲行里的字符（否则会得到 `hii`）；
3. `keyboard+tty ok` —— 程序正常收尾。

测试通过 QMP `send-key` 注入（`tools/qemu-test.sh` 的 `--send-keys`），所以断言是**端到端**的：从 QEMU 虚拟键盘到中断、队列、行缓冲。无像素断言。

## 对照阅读

- `source/kernel/chr_drv/keyboard.S` —— 0.11 的键盘中断全程汇编：`key_table` 分发表 + `key_map`/`shift_map` 等多维键位表 + `put_queue`（对应本课 `q_put`）。它还处理 CapsLock/NumLock、重置探测等，课程版只保留主干。
- `source/kernel/chr_drv/tty_io.c` —— `read_chan()` 的 canonical 行规则（退格擦 `read_q`、回车唤醒等待进程）是本课"读取方行收集"的 0.11 原型；`tty_read` 则是上层接口。
- `source/kernel/chr_drv/rs_io.s`：串口驱动同属 chr_drv，与本课互为镜像（一个收键一个收字节）。

## 实验

1. **支持大写输入**：把测试键序列里加一个 shift（QMP 键名 `shift`），或先在 `keymap` 里给数字行加上符号列——体验一下为什么 0.11 需要 `shift_map`、`alt_map` 好几张表。
2. **队列满了会怎样**：把 `QLEN` 改成 8，测试注入一长串按键，观察被丢弃的字符让 `line=` 缺字——然后想想 0.11 队列满时的策略（`tty_io.c` 里字符直接丢弃并统计），以及"流控"存在的理由。
3. **回显开关**：给 `tty_init` 加一个全局 `echo_on` 标志，关闭后预测测试现象（`line='hi'` 仍在——回显只影响屏幕不影响队列；但日志里会看不到键入的字符）。
