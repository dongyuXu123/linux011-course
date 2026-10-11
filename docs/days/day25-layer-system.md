# day25 - layer-system：图层 z 序与整屏刷新

窗口系统的心脏。`kernel/desktop.c` 引入图层（sheet）模型：`win_create` 在数组里登记窗口（位置/尺寸/标题色/可见性），数组下标即 z 序——背景（自绘渐变+棋盘格底纹）在最底、窗口按数组顺序叠放、光标精灵画在最顶。`gui_refresh(cursor_x, cursor_y)` 每次**自底向上整屏重绘**。主循环只在光标坐标变化时才刷新（脏检查）。这是 30dayMakeOS day10 sheet 系统的教学化翻版；0.11 没有图形栈，属于课程化扩展。

本课光标精灵从 day24 的"自管背景块"改为**每次刷新最后画**——图层模型下不再需要手动保存/恢复背景，重绘天然解决遮挡。测试注入鼠标位移（`MOUSE := 100,120`）让光标横穿两个窗口。串口输出：

```
[day25] framebuffer 640x480 pitch=2560 at fd000000 (mapped)
[day25] layer system online
[day25] first refresh done
[day25] cursor at (420,360) btn=0
```

## 知识点

- **z 序即数组序**：`wins[MAXWINS]` 的下标 0..n-1 就是叠放层次，`gui_refresh` 按 `for i in 0..nwins` 顺序绘制——后来者自然盖住先来者。"把窗口提到最顶"在未来（day26）等价于把它移到数组末尾，O(n) 交换而已。
- **整屏重绘（画家算法）**：每帧从背景到光标全画一遍。640×480×32bit ≈ 1.2MB/帧，QEMU 内存带宽下毫秒级完成。它的优点是**状态一致无残影**（不需要增量补偿逻辑），缺点是浪费——day28 会用脏标记把重绘降到"内容变化时"。
- **脏检查驱动**：主循环比较 `cursor_x/y` 前后值，动了才 `gui_refresh`。把"何时重绘"与"如何重绘"分离，是所有 GUI 框架（从 X 到浏览器）共有的分层。
- **背景也是图层**：渐变 + `((x/32)+(y/32))%2` 棋盘格底纹在每次刷新时重算——把背景做成"第一个图层"而非一次性初始化，窗口移动/删除后才不会留下永久痕迹。
- **光标成为最高层**：day24 的精灵要手工保存/恢复背景；图层模型下光标只是"最后画的那一层"，遮挡、移动、跨窗口都由重绘机制统一解决——抽象层次提高后，代码反而更短。
- **窗口数上限与登记制**：`MAXWINS 4`，`win_create` 返回 id 或 -1——静态数组换来了零分配（不需要 kmalloc）与 O(1) 的遍历；day29 的应用窗口、day28 的终端窗口都从同一个池子里领 id。

## 关键代码走读

（本课路径均相对 `days/day25-layer-system/`）

`kernel/desktop.c` 的图层结构：

```c
#define MAXWINS 4
struct win {
    int x, y, w, h;
    uint32_t title_rgb;
    const char *title;
    int visible;
};
static struct win wins[MAXWINS];
static int nwins = 0;
```

`kernel/desktop.c` 的 `gui_refresh` —— 自底向上整屏重绘：

```c
/* 背景: 渐变 + 底纹格子 */
for (y...) for (x...) {
    uint32_t c = ((x/3) << 16) | ((y/3) << 8) | 0x402030;
    if (((x/32) + (y/32)) % 2 == 0)
        c = ((x/3)>>1 << 16) | ((y/3)>>1 << 8) | 0x302838;
    fb_pixel(x, y, c);
}
for (int i = 0; i < nwins; i++)            /* 窗口按 z 序叠放 */
    if (wins[i].visible)
        gfx_window(wins[i].x, wins[i].y, ...);
draw_cursor(cursor_x, cursor_y);           /* 光标最顶 */
```

`kernel/desktop.c` 的 `win_create/win_move`：登记与平移（z 序不变）；`kernel/main.c` 主循环：`if (cursor_x != ox || cursor_y != oy) gui_refresh(...)`。

`kernel/desktop.c` 的 `draw_cursor` —— 光标作为最后一层直接画（不再保存/恢复背景）：

```c
static void draw_cursor(int cx, int cy)
{
    for (j...) for (i...) {
        if (arrow[j][i] == 'B') fb_pixel(cx + i, cy + j, 0);
        else if (arrow[j][i] == 'W') fb_pixel(cx + i, cy + j, 0x00FFFFFF);
    }
}
```

## 验收

`test/expect.txt` 四行顺序断言：

1. `layer system online` —— 图层模块初始化；
2. `first refresh done` —— 首次 `gui_refresh` 完成（背景+两窗口+光标全链路）；
3. `cursor at (420,360) btn=0` —— 注入位移后光标坐标精确到达（day24 的包解析在图层模型下回归通过）；
4. 顺序本身同时验证：刷新必须发生在鼠标更新之后（否则坐标对不上）。

像素断言（`PIXEL := 380,68=2060c0; 390,180=e8e8e8`）：分别采样 "window A" 标题条（蓝）与内容区（灰）——确认窗口在**整屏重绘后**颜色仍然精确，即刷新不累积色偏、不漏绘。（检查器现状说明见 day21；截图人工复核光标与窗口叠放。）

## 对照阅读

- 30dayMakeOS **day10**（叠加处理）：`struct SHEET`/`sheet_updown/sheet_refresh/sheet_slide`——课程 `struct win` 的原型；haribote 还做了"只刷新窗口矩形"的局部优化，本课刻意先做全屏版，把优化留给脏标记（day28）。
- 30dayMakeOS day11：`sheet_slide` 处理窗口/光标移动的增量路径，与本课"移动后整刷"对照。
- 0.11 无对应物（无图形栈）；`source/kernel/chr_drv/console.c` 的滚屏是"字符层重绘"的最近亲。

## 实验

1. **窗口提层**：实现 `win_top(id)`——把该窗口与数组末尾元素交换再刷新；用鼠标位移到某窗口上后调用它，截图对比遮挡关系变化。
2. **局部刷新优化**：给 `gui_refresh` 加一个"只重绘光标新旧位置围成的矩形并集"的 `gui_refresh_rect` 模式，用计数器统计每秒刷过的像素数对比——亲手把 O(全屏) 降到 O(光标区)。
