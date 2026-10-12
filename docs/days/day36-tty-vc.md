# day36 - TTY 行规程 + 多控制台(0.11 tty_io/console)

day08 的 tty 是"字符直通队列"。今天补上 0.11 的**行规程**
(canonical mode): 键盘字符先进规程缓冲, 退格擦字、^U 清行、回显,
回车才把整行交给 read() —— read 的单位从此是行而不是字符。
同时加上**双虚拟控制台**: 显存一份, 每 VC 一块内存缓冲, Alt-F1/F2
整卷切换, 规程焦点随控制台走。

## 已实现并验证(test 23/23)

- `kernel/tty.c`(新): `tty_ldisc_putc`(IRQ 入口: 缓冲/退格/^U/回显/
  回车置就绪), `tty_ldisc_read`(阻塞取整行, 0.11 read_chan 课程版),
  `tty_ldisc_focus`(焦点随 VC)
- `console.c`: `vcbuf[2][25x80]` + `con_switch`(缓冲整卷搬上显存,
  0.11 set_origin 的简化) + `vc_peek`(自检窥视)
- `keyboard.c`: 字符全交规程; Alt+F1/F2 → ldisc 焦点 + con_switch;
  day08 的原始环形队列退役
- `sys_read` fd==0 改为规程整行交付(0.11 canonical 语义)
- 自检: 程序化喂 "ab\bc\n" 得 "ac\n"、^U 清行后 "d\n";
  VC1 写入 VC1-LIVE 后切回, vc_peek 验证两缓冲互不干扰
- 端到端: 键入 "run hel" + backspace + "lo" → 修正为 "run hello"
  → fork/execve/demand-paging 全流程照旧绿

## 调试实录

测试键序 `l,l,l,backspace,l,o` 打出了 "helllo" —— 数 l 的个数发现
退格生效了, 是**测试键序本身多打了一个 l**( intended "hello" 应为
h,e,l,l,bsp,l,o)。教训: 键序类测试的期望行要按"逐键重放"手工推演,
别凭感觉写。另外 QEMU qcode 是 `backspace`(无 `bsp` 别名时需实测),
send-key 失败只有 WARN 且被 make 吞掉 —— 键序断言失败先查 WARN。

## 对照阅读

- `source/kernel/chr_drv/tty_io.c`: read_chan 的规范模式循环
  (copy_to_cooked 做退格/^U/回显加工)
- `source/kernel/chr_drv/console.c`: vc_cons[:] 与 set_origin
- `include/linux/tty.h`: tty_struct 里 read_q/secondary 的分工

## 课程简化

- 规程缓冲 64B、两条(0.11 每 tty 一套 read_q+secondary)
- 无 tab 展开/回显列退避; 无 raw 模式; 交互式切换仅 Alt-F1/F2
