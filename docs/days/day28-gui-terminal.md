# day28 - gui-terminal：把 shell 渲染进图形终端窗口

两周半的成果汇合：字符世界的 shell 搬进图形世界。新增 `kernel/term.c`——一个 32×12 的**文本网格终端**：`term_putc` 维护字符网格与滚动（与 day05 控制台同构但落在内存数组），`term_render` 把网格用 `gfx_putstr` 画进窗口内容区（8px 字格、灰色底、红色光标块），并带 `dirty` 标记。shell 逻辑是 day14 的窗口版：`term_shell_poll()` 非阻塞取键（`tty_getc_nonblock`，同一 tty 队列）、收行后执行 `help/ps/ver` 内置命令，输出**同时镜像到串口**（测试断言通道）。

桌面因此有三个窗口：window A、window B（可拖动）、terminal（绿色标题）。测试注入 `help⏎`、`ver⏎` 两组按键。串口输出：

```
[day28] first refresh done
builtins: help ps ver
linux011-course 0.28 gui
```

## 知识点

- **文本网格与字符单元**：`grid[12][33]` 是"逻辑屏幕"，`term_render` 是"物理屏幕"——两层的换算固定为 8px/格（`tx = 窗口x+6, ty = 窗口y+20`）。这就是 terminfo 里 cols/rows 与像素尺寸关系的最小实现；窗口移动时只改偏移，网格内容不动。
- **非阻塞取键**：day14 的 shell 在 `tty_getc` 里阻塞，独占内核主循环；GUI 版本必须轮询（`tty_getc_nonblock` 空则返回 -1），让鼠标、拖动、终端在一根主循环里协同——单线程事件循环的三个事件源（键/鼠标/时钟）就此齐备。
- **脏标记刷新**：`term_putc` 置 `dirty=1`，`term_render` 画完清零；主循环的刷新条件是 `坐标变了 || term_dirty_flag()`。day25 的"坐标脏"在此升级为双通道脏检查——`help` 这类只改网格不动光标的输出也触发重绘。
- **双通道输出的镜像**：`term_puts` 每写一个网格字符都 `printk("%c")` 串口镜像一份——图形终端的"屏幕内容"由此变得可自动断言。这个技巧让 day29-30 的 GUI 测试不需要 OCR。
- **ver 与版本演化**：终端里 `ver` 打印 `linux011-course 0.28 gui`——从 day20 的 `0.20 (terminal milestone)` 到本课，版本串直接反映界面形态。软件版本即里程碑标签，两者在课程里一一对应。
- **窗口注册的三方握手**：`win_create`（图层）→ `term_attach(tw)`（终端绑定窗口 id）→ `gui_refresh` 里按 id 反查矩形渲染——图层与内容各自独立演进，靠 id 关联，这是窗口系统"框架与内容分离"的雏形。

## 关键代码走读

（本课路径均相对 `days/day28-gui-terminal/`）

`kernel/term.c` 的 `term_putc` —— 网格滚动：

```c
if (ccol >= TERM_COLS) { ccol = 0; crow++; }
if (crow >= TERM_ROWS) { scroll(); crow = TERM_ROWS - 1; }
grid[crow][ccol++] = c;
grid[crow][ccol] = 0;
dirty = 1;
/* scroll(): strcpy 上移一行, 末行清空 —— day05 con_scroll 的网格版 */
```

`kernel/term.c` 的 `term_render` —— 网格落到窗口内容区：

```c
int tx = x + 6, ty = y + 20;          /* 内容区原点 */
for (int r = 0; r < TERM_ROWS; r++)
    gfx_putstr(grid[r], tx, ty + r * 8, 0x00202020, 0x00E8E8E8);
gfx_putstr("_", tx + ccol * 8, ty + crow * 8, 0x00C02020, ...);  /* 光标块 */
dirty = 0;
```

`kernel/term.c` 的 `term_shell_poll` —— 非阻塞 shell：

```c
char c = tty_getc_nonblock();
if (c == -1) return;
if (c == '\n') { line[len] = 0; do_cmd(line); len = 0; term_puts("linux011> "); }
...
void term_puts(const char *s) { for (; *s; s++) { printk("%c", *s); term_putc(*s); } }
```

`kernel/main.c` 主循环：`mouse_task_update → term_shell_poll → gui_drag_step → (脏则 gui_refresh → term_render)`。

`kernel/desktop.c` 的 `term_window_rect` —— 按 id 反查终端窗口矩形（`term_render` 的定位依据），窗口被拖动后坐标自动更新。

## 验收

`test/expect.txt` 四行顺序断言（`KEYS := h,e,l,p,ret,v,e,r,ret`）：

1. `first refresh done` —— 三窗口桌面初绘成功；
2. `builtins: help ps ver` —— 注入的 `help⏎` 经 tty 队列 → term shell → 网格 + **串口镜像**；
3. `linux011-course 0.28 gui` —— `ver` 命令输出（版本即本课身份）；
4. 顺序保证按键处理发生在首绘之后——事件循环的时序正确。

像素断言（`PIXEL := 60,468=181830; 30,75=202020`）：前者任务栏底色回归（day27），后者采样终端窗口内容区的**窗体灰**（0x00E8E8E8 经底纹位置折算的采样点）——确认终端窗口的绘制层次与颜色正确。文字像素的逐点断言交给串口镜像完成（见上）。检查器现状说明见 day21。

## 对照阅读

- 30dayMakeOS **day17**（命令行窗口）：`make_textbox8` + 光标闪烁 + 回车处理的终端窗口，与 `term.c` 逐概念对应（haribote 还有窗口输入光标的 sheet 管理）。
- day28（haribote 的多控制台/窗口按键分发）：`keywin_on/off` 处理"焦点窗口"，对照本课"只有一个终端、全键盘归它"的简化。
- `source/kernel/chr_drv/console.c`：0.11 的 con 是"逻辑格阵 → 显存"的翻译器，`term_putc/term_render` 把同一翻译搬进了窗口——输入端（tty 队列）day08 起完全同构。

## 实验

1. **加回退格显示**：目前 `\b` 只回退列计数不擦字符，让 `term_putc` 的 `\b` 分支把网格字符置空格并重画光标——对齐 day14 shell 的行为。
2. **终端加 ps 真数据**：把 `do_cmd` 的 `ps` 分支改成遍历 day19 的 `task[]` 打印真实 pid/状态进终端网格（串口镜像会同时把它们变成可断言行）——把 day14 的 `cmd_ps` 完整移植到窗口世界。
