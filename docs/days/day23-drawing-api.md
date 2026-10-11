# day23 - drawing-api：矩形/线段绘图基元与窗口框打样

图形 API 层成型。`kernel/gfx.c` 在字体渲染之下补齐绘图基元：`fb_rect`（实心矩形）、`fb_hline/fb_vline`（水平/垂直线）、`fb_line`（**Bresenham** 任意角度直线，整数增量、无乘除）、`gfx_window`（投影 + 窗体 + 标题条 + 高光的窗口框）。`kmain` 的演示：8 条 Bresenham 扇形线从 (90,420) 放射，画两个窗口框（"window demo" 蓝色标题、"another window" 棕色标题）——为 day26 的窗口系统打样。

串口输出：

```
[day23] framebuffer 640x480 pitch=2560 at fd000000 (mapped)
[day23] gradient painted on LFB
[day23] drawing primitives demo
[day23] lines + window frames drawn
```

## 知识点

- **Bresenham 直线**：以误差项 `err = dx - dy` 迭代，每步按 `e2 = 2*err` 与 ±dx/±dy 的比较决定 x 前进、y 前进或双进——全程整数加减，无浮点无除法。这是 1980 年代至今所有软件光栅器的看家算法；haribote 的 `drawline` 同源。
- **矩形 = 双层循环的 fb_pixel**：最慢但最诚实的实现。优化方向有二：行内连续像素按 pitch 合并写（memcpy 语义）、`fb_fill` 按行展开——课程保持逐像素以保证 `fb_pixel` 的边界检查统一生效（越界剪裁在这一层集中处理）。
- **窗口框的图层语义预演**：`gfx_window` 的绘制顺序（先画投影矩形、再窗体、再标题条、最后高光线）就是"后画者在上"的画家算法雏形；投影矩形 `fb_rect(x+3, y+3, ...)` 画的是**全黑**，叠在渐变上制造阴影深度。
- **gdi 心智模型**：`fb_rect/fb_line/gfx_window` 构成最小绘图原语集——后续 day25-30 的所有 UI（窗口、任务栏、时钟、应用）都只用这三个原语 + putstr 组合出来。API 面小而正交，是它能稳定支撑六天演进的原因。
- **颜色即 0x00RRGGBB 常量**：`0x00E8E8E8` 窗体灰、`0x002060C0` 标题蓝、`0x00A04020` 标题棕——主题色集中为常量，day27 起换主题只改一处。

## 关键代码走读

（本课路径均相对 `days/day23-drawing-api/`）

`kernel/gfx.c` 的 `fb_line` —— Bresenham：

```c
int dx = x1 - x0, dy = y1 - y0;
int sx = dx > 0 ? 1 : -1, sy = dy > 0 ? 1 : -1;
dx = dx > 0 ? dx : -dx;  dy = dy > 0 ? dy : -dy;
int err = dx - dy;
for (;;) {
    fb_pixel(x0, y0, rgb);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 > -dy) { err -= dy; x0 += sx; }
    if (e2 <  dx) { err += dx; y0 += sy; }
}
```

`kernel/gfx.c` 的 `gfx_window` —— 窗口框四步：

```c
fb_rect(x + 3, y + 3, w, h, 0x00000000);   /* 投影 */
fb_rect(x, y, w, h, 0x00E8E8E8);           /* 窗体 */
fb_rect(x, y, w, 16, title_rgb);           /* 标题条 */
fb_hline(x, y + h - 1, w, 0x00888888);     /* 底边高光 */
if (title) gfx_putstr(title, x + 6, y + 4, 0x00FFFFFF, title_rgb);
```

`kernel/main.c` —— 扇形与两扇窗口：

```c
for (int i = 0; i < 8; i++)
    fb_line(90, 420, 90 + i * 60, 320, 0x000000A0);
gfx_window(380, 60, 220, 130, 0x002060C0, "window demo");
gfx_window(60, 300, 260, 120, 0x00A04020, "another window");
```

## 验收

`test/expect.txt` 四行顺序断言：帧缓冲就绪（`at fd000000 (mapped)`）→ 渐变 → `drawing primitives demo` → `lines + window frames drawn`。

像素断言（`PIXEL := 380,68=2060c0; 390,180=e8e8e8; 90,420=a0`）：

- `380,68=2060c0` —— "window demo" 标题条内部的标题蓝色（窗口框的存在性与配色）；
- `390,180=e8e8e8` —— 同一窗口的**内容区**窗体灰（标题条 16px 之下的分层正确）；
- `90,420=a0` —— 扇形放射线的**起点像素**深蓝（Bresenham 起点与颜色低字节；线体覆盖处 R/G 通道被线色 0 拉低，只剩 B=0xa0）。

（像素检查器现状说明见 day21；人工复核看 `build/test/screen.png`。）

## 对照阅读

- 30dayMakeOS **day09-10**（叠加处理/窗口渲染）：haribote 的 `boxfill8`（矩形）、`init_screen`（画桌面色块与"任务栏"）对应本课 `fb_rect/gfx_window`；它的 `putfonts8_asc` 与 day22 对应。
- 30dayMakeOS day11（`make_window8`）：真正带边框/关闭按钮的窗口绘制函数，是本课 `gfx_window` 的加强版（多一层按钮热区）——day26 将借用其思想。
- Bresenham 原始论文（1965）与 0.11 无涉（0.11 无图形），这条算法线属于"经典图形学"而非 OS 内核——课程在这里交叉引用两条传统。

## 实验

1. **加圆形基元**：实现 `fb_circle(cx, cy, r, rgb)`（中点圆算法，`d = 1 - r` 迭代），在扇形圆心处画一个圆——检查你的实现与 Bresenham 一样只用整数运算。
2. **窗口框加边框色**：把 `gfx_window` 改成双层边框（外 1px 深色 + 内 1px 亮色），重跑像素断言并新增两个采样点——练习"改 API 必须同步改验收坐标"的工程习惯。
