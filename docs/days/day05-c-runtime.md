# day05 - c-runtime：VGA 文本控制台与 C 运行环境

给裸机 C 内核补上"生活基础设施"。本课写第一个正经的输出子系统：`kernel/console.c` 实现一个 80×25 的 VGA 文本控制台——支持换行、回车、退格、**整屏滚动**，并把硬件光标（CRTC 端口）挪到正确位置；同时每个字符都镜像到 COM1，串口通道与屏幕从此永远同步。`kernel/main.c` 连刷 40 行逼出滚动，验证行数超过 25 后旧行上移、光标停在底部。

屏幕上（`make run`）可以看到 40 行 `scroll line N` 滚过、最后一行停在屏幕底部；串口上是三行断言输出：

```
[day05] VGA text console ready (80x25)
[day05] scroll line 40
[day05] scroll test done, cursor should be at bottom
```

## 知识点

- **VGA 文本模式布局**：显存 `0xB8000` 起，每字符 2 字节 `[ASCII][属性]`，80 列 × 25 行共 4000 字节。属性字节高 4 位背景、低 4 位前景，`0x07` 是黑底白字。
- **滚动的本质是 memcpy**：没有任何硬件帮忙——把第 1~24 行整体前移一行（40×2 字节 × 24 次 `uint16_t` 拷贝），最后一行填空格。0.11 的 `console.c` `con_scroll` 用的也是同一思路（它还要处理 `scr_top` 与不同显存起点）。
- **硬件光标编程**：CRT 控制器（CRTC）索引/数据口 `0x3D4/0x3D5`。光标位置是 16 位线性偏移（`row*80+col`），分两次写：索引 `0x0E` 写高字节、`0x0F` 写低字节。每次 `con_putc` 后都同步一次，光标才能"贴着"最后输出的字符。
- **BSS 清零与 C 静态变量**：`console.c` 的 `cur_row/cur_col` 是静态变量，落在 `.bss`。day04 的 aout-kludge 让 GRUB 清 `[load_end, bss_end)`；day06 的 `head.S` 还会再清一次双保险。没有这步，所有"默认为 0"的变量都是内存残渣。
- **串口镜像设计**：`con_putc` 第一行就 `serial_putc(c)`——所有高层输出（此后的 printk、shell、应用）自动带上串口副本，测试断言通道与显示层彻底解耦。这是整个课程可测性的基石。

## 关键代码走读

（本课路径均相对 `days/day05-c-runtime/`）

`kernel/console.c` 的 `con_scroll` —— 整体上移一行：

```c
static void con_scroll(void)
{
    for (int i = 0; i < (ROWS - 1) * COLS; i++)
        vram[i] = vram[i + COLS];
    for (int i = (ROWS - 1) * COLS; i < ROWS * COLS; i++)
        vram[i] = (uint16_t)(cur_attr << 8) | ' ';
}
```

`kernel/console.c` 的 `con_update_cursor` —— CRTC 光标分两次写：

```c
uint16_t pos = cur_row * COLS + cur_col;
outb(0x3D4, 0x0F);
outb(0x3D5, (uint8_t)(pos & 0xFF));   /* 低字节 */
outb(0x3D4, 0x0E);
outb(0x3D5, (uint8_t)(pos >> 8));     /* 高字节 */
```

`kernel/console.c` 的 `con_putc` —— 串口镜像 + 特殊字符：

```c
void con_putc(char c)
{
    serial_putc(c);                       /* 一切输出镜像到串口 */
    if (c == '\n') { cur_col = 0; cur_row++; }
    else if (c == '\b') { if (cur_col > 0) { cur_col--; ...擦掉 } }
    else { vram[cur_row*COLS + cur_col] = ...; if (++cur_col >= COLS) 换行; }
    if (cur_row >= ROWS) { con_scroll(); cur_row = ROWS - 1; }
}
```

`kernel/main.c` 的 `puts_dec` —— 手写的 int→十进制（无 libc 的 `sprintf` 可用），day09 会被完整的 vsprintf 取代。

## 验收

`test/expect.txt` 三行顺序断言：

1. `VGA text console ready (80x25)` —— `con_init` 完成（清屏 + 光标归零）；
2. `scroll line 40` —— 循环刷到第 40 行仍未崩溃，说明滚动路径被真实触发（第 26 行就开始滚）；
3. `scroll test done, cursor should be at bottom` —— 40 行滚完后光标管理没有把行列算飞。

本课无像素断言；`build/test/screen.png` 可人工复核"最后一行贴底、旧行上移"。

## 对照阅读

- `source/kernel/chr_drv/console.c` —— 0.11 的控制台：`con_write()` 从 tty 队列取字符、`con_scroll()`、`gotoxy()/set_cursor()` 与本课一一对应；差别在于 0.11 支持 ESC 转义序列和多种显存起点（CGA/MDA），课程版刻意精简。
- `source/kernel/chr_drv/tty_io.c`：0.11 里控制台是 tty 的下层；day08 接上键盘后，本课的 `con` 层会以同样方式被 tty 层调用。
- `source/include/linux/tty.h` 的 `struct tty_struct`：可以看到 0.11 把"列/行/属性"作为 tty 的字段管理。

## 实验

1. **实现 `\t` 制表符**：在 `con_putc` 加一个分支：遇到 `\t` 把 `cur_col` 对齐到 8 的倍数（`(cur_col & ~7) + 8`），注意超过 80 列时要触发换行——顺手给 day09 的格式化输出铺路。
2. **滚动丢历史**：目前滚动是直接覆盖，没有回滚。把每行被滚出的内容也 `serial_putc` 一份（串口即成"无限回滚缓冲"），思考真正的回滚缓冲（0.11 没有）需要多大内存。
