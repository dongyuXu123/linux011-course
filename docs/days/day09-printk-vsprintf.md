# day09 - printk-vsprintf：把 0.11 的 vsprintf/printk 移植进内核

内核日志的地基。本课把 Linux 0.11 的 `vsprintf.c`（1991 年 Lars Wirzenius 与 Linus 的作品，遵循原仓库 LICENSE 随课程分发）移植到现代 GCC——`do_div` 内联汇编换成显式 64 位除法、`stdarg` 用编译器内建——并在其上实现 `printk()`（vsprintf → `con_puts`，串口镜像自动带上）与 `panic()`。`kmain` 跑一组格式化自测：有符号/十六进制/字符串/字符、零填充与左对齐、精度截断、`%%`。

串口输出：

```
[day09] dec=-42 hex=0x2a str=linux011 char=Z
[day09] pad [00042] [    7] [3    | end
[day09] str [  abc] [abc  ] [ab]
[day09] 100% complete, vsprintf ok
```

## 知识点

- **vsprintf 的结构**：主循环逐字符扫格式串；遇 `%` 依次解析 flags（`-0+ #`）→ 域宽（数字或 `*`）→ 精度（`.数字`）→ 修饰符（`h/l/L`），最后按转换符分发。整数渲染收拢进 `number()` 子过程，从低位到高位产出数字再倒序输出——0.11 原版逐行对应，移植只动了语法层。
- **flags 位掩码**：`ZEROPAD/SIGN/PLUS/SPACE/LEFT/SPECIAL/SMALL` 与 0.11 同名同值。`LEFT` 要压制 `ZEROPAD`（左对齐时不能补 0）、`SPECIAL` 提前扣掉 `0x`/`0` 前缀的宽度——这些"看似琐碎"的交互正是格式化器的全部复杂度。
- **0.11 的 do_div 去哪了**：GCC 1.40 时代 32 位机做 64 位除法必须内联汇编（除数放低 32 位、余数回填）；现代 gcc -m32 直接把 `num % base`、`num /= base` 编成 `__udivdi3` 或移位优化。移植的价值在于：**行为不变，平台胶水随工具链换代**。
- **printk 与用户 printf 的差别**：没有 stdio 缓冲、没有 stdout 概念，直接落到 `con_puts`；静态 1KB 格式化缓冲意味着并发打印可能交错——0.11 里 printk 靠 `tty_write` 走队列，课程内核单线程无此需求，day11 出现多任务后要靠串口镜像的原子性（每字符一次 outb）兜底。
- **panic 语义**：格式化后打 `Kernel panic: ...` 然后 `cli; hlt` 永久停机。0.11 的 `panic.c` 还会同步缓冲、死循环——"报完就停"是内核开发期最诚实的失败方式。

## 关键代码走读

（本课路径均相对 `days/day09-printk-vsprintf/`）

`kernel/vsprintf.c` 的 `number()` —— 整数渲染核心（对照 `source/kernel/vsprintf.c` 同名函数）：

```c
if (type & SIGN && (int)num < 0) { sign = '-'; num = -(int)num; }
...
while (num != 0) {
    tmp[i++] = digits[num % (unsigned)base];   /* 0.11: do_div 宏 */
    num /= (unsigned)base;
}
```

`kernel/vsprintf.c` 的转换符分发 —— `%x` 落到 `SMALL`+base16：

```c
case 'x':
    flags |= SMALL;
    /* fall through */
case 'X':
    base = 16;
    goto number_case;
```

`kernel/printk.c` 的 `printk` —— 三行内核：

```c
va_start(args, fmt);
i = vsprintf(buf, fmt, args);
va_end(args);
con_puts(buf);                    /* 串口镜像由 con 层负责 */
```

`kernel/main.c` 的自测样例：

```c
printk("[day09] pad [%05d] [%5d] [%-5d| end\n", 42, 7, 3);
printk("[day09] str [%5.3s] [%-5s] [%.2s]\n", "abcdef", "abc", "abc");
```

## 验收

`test/expect.txt` 四行顺序断言——每行都是一组格式化用例的精确期望值：

1. `dec=-42 hex=0x2a str=linux011 char=Z` —— %d 符号处理、%p/十六进制、%s、%c；
2. `pad [00042] [    7] [3    | end` —— 零填充（%05d）、右对齐（%5d）、左对齐（%-5d）三种填充方向；
3. `str [  abc] [abc  ] [ab]` —— 精度截断（%5.3s）与左对齐组合；
4. `100% complete, vsprintf ok` —— `%%` 转义与收尾。

任何一处的填充/截断算错，期望行就对不上。无像素断言。

## 对照阅读

- `source/kernel/vsprintf.c` —— 移植母本：`number()`、`skip_atoi()`、flags 常量、`case 's'` 里 `if (!s) s="(null)"` 全部在列；文件头保留 Lars 与 Linus 的版权声明。
- `source/kernel/printk.c` —— 0.11 的 printk：`va_args` 后走 `tty_write`（把输出交给 tty 层排队），与本课"直达 con 层"的差异正好复习 day08 的分层。
- `source/kernel/panic.c` —— `sys_sync()` + 死循环；`source/include/stdarg.h`（0.11 用 GCC 内建 `__builtin_*args`，与课程移植后的选择一致）。

## 实验

1. **补一个 %b 二进制转换**：在 switch 里加 `case 'b': base = 2;`，然后打印 `printk("%b", 0b1010)`——注意 `number()` 的 digits 表天然支持 base 2~36，只需放开上限判断。
2. **找出一个 0.11 与现代语义的差异**：0.11 的 `%p` 没有 `0x` 前缀且零填充到 8 位，本课加了前缀——试着改回 0.11 行为再跑测试，用 expect 失败来确认你理解了每一行断言在测什么。
