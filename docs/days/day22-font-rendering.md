# day22 - font-rendering：8×16 位图字体与 putfont/putstr

屏幕从此"会说话"。新增 `include/font8x8.h`——公有领域（dhepper/font8x8）的 ASCII 点阵字库，每字符 8 字节、每 bit 一个像素；`kernel/gfx.c` 提供 `gfx_putfont`（把一个字形的 8×8 位展开成前景/背景像素）与 `gfx_putstr`（逐字符步进 8 像素，天然支持背景色块叠加）。`kmain` 在渐变背景上先画一条深色标题条，再用白色/青绿色渲染两行横幅文字。

串口输出：

```
[day22] framebuffer 640x480 pitch=2560 at fd000000 (mapped)
[day22] gradient painted on LFB
[day22] banner text rendered via font8x8
```

屏幕（截图 `build/test/screen.png`）上可见深蓝条上的白色标题 `linux011-course day22 - font rendering` 与第二行青绿色样张 `ABCDEFG abcdefg 0123456789 !@#$%`。

## 知识点

- **点阵字体的数据结构**：`font8x8_basic[128][8]`——128 个可打印 ASCII，每个 8 字节按**行**存储，每行的 8 个 bit 按列展开。`gfx_putfont` 的双层循环是"位图 → 像素"的标准翻译：`(bits >> col) & 1` 选前景色，否则背景色。
- **前景/背景一次成型**：putfont 不是"画字"，而是**替换**整块 8×8 区域的颜色（前景+背景一起写）。这使文字天然有底色、能盖住下面的渐变——day25 的图层系统复用这招把窗口内容"贴"上去。
- **8 像素步进即字符网格**：`gfx_putstr` 里 `x += 8`，任意字符串天然对齐到一个 8px 网格；`gfx_strlen_px` 返回 `字符数 × 8` 供居中/右对齐计算。day27 任务栏的按钮宽度、day28 终端的列宽都建立在这个网格上。
- **与 VGA 文本模式的对比**：day05 的文本模式由字符发生器硬件把"字符码+属性"翻译成点阵，内核只写 2 字节；帧缓冲模式下翻译由软件做，代价是每字符 64 次像素写（8×8），收益是颜色/位置/字号完全自由。
- **kmain 的固化快照策略**：图形课的 `kmain` 画完就 `hlt` 常驻——串口打"进度日志"，屏幕状态留给截图与像素断言。这套"串口叙事 + 屏幕终态"的组合拳是 day21-30 的统一测试模型。

## 关键代码走读

（本课路径均相对 `days/day22-font-rendering/`）

`kernel/gfx.c` 的 `gfx_putfont` —— 位图展开：

```c
const unsigned char *glyph = font8x8_basic[(unsigned char)ch & 0x7F];
for (int row = 0; row < 8; row++) {
    unsigned char bits = glyph[row];
    for (int col = 0; col < 8; col++)
        fb_pixel(x + col, y + row,
                 (bits & (1 << col)) ? fg : bg);
}
```

`kernel/gfx.c` 的 `gfx_putstr` —— 网格步进：

```c
while (*s) {
    gfx_putfont(*s++, x, y, fg, bg);
    x += 8;
}
```

`kernel/main.c` —— 标题条 + 双色横幅：

```c
for (int y = 8; y < 28; y++)          /* 深色条 (8,8)-(360,28) */
    for (int x = 8; x < 360; x++)
        fb_pixel(x, y, 0x00101030);
gfx_putstr("linux011-course day22 - font rendering", 16, 12,
           0x00FFFFFF, 0x00101030);
gfx_putstr("ABCDEFG abcdefg 0123456789 !@#$%", 16, 32,
           0x0000FF88, 0);
```

## 验收

`test/expect.txt` 三行顺序断言：帧缓冲就绪（`at fd000000`）→ 渐变绘制 → `banner text rendered via font8x8`。

像素断言（`PIXEL := 260,240=ff8800; 20,15=101030; 17,15=ffffff`）：

- `260,240=ff8800` —— day21 遗留的中央橙色矩形仍完好（文字绘制没有误伤别处）；
- `20,15=101030` —— 标题条的**背景色**精确（深蓝 0x101030）；
- `17,15=ffffff` —— 标题第一个字符 `l` 的笔画落在前景白色上：一个像素点同时证明字体表取址、位展开、颜色选择全链路正确。

（像素检查器的现状说明见 day21 验收节；本课同样受其影响，人工复核看 `build/test/screen.png`。）

## 对照阅读

- 30dayMakeOS **day07-08**（字符显示）：haribote 的 `hankaku.txt` 256 字符 8×16 字库经 `make` 转成 `hankaku.o`，`putfont8/putfonts8_asc` 与本课 `gfx_putfont/gfx_putstr` 一一对应——连"字库存成目标文件而不是运行时读文件"都一样（课程用 `#include` 头文件，haribote 用 obj 链接）。
- 30dayMakeOS day09（`putblock8_8`）：按块拷贝的思路将在 day25 图层搬移时用到。
- `source/kernel/chr_drv/console.c`：0.11 依赖 VGA 字符发生器的硬件字库，软件里只有"写字符码"——对比理解本课为什么每字符要多写 64 倍的像素。

## 实验

1. **做一版 8×16 高字库**：把 `font8x8` 纵向拉成 8×16（每行重复一次即可先糊弄出"高字"），观察可读性变化；再想想真正的 8×16 字库（VGA 默认字库）每字符要 16 字节的原因。
2. **实现居中打印**：用 `gfx_strlen_px` 写一个 `gfx_putstr_center(s, y, fg, bg)`，把标题条上的文字居中——顺手验证 640 宽、8px 网格、条宽 352 之间的整除关系。
