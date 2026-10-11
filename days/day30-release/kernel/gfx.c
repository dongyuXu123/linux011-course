/* =====================================================================
 * day22 - kernel/gfx.c：点阵字体与文本渲染
 * font8x8(公有领域) 的每个字符 8 字节, 每位一个像素;
 * putfont 逐位展开, putstr 叠加背景色块 —— 屏幕从此"会说话"。
 * ===================================================================== */
#include <kernel.h>
#include <font8x8.h>

void fb_pixel(int x, int y, uint32_t rgb);

void gfx_putfont(char ch, int x, int y, uint32_t fg, uint32_t bg)
{
    const unsigned char *glyph = font8x8_basic[(unsigned char)ch & 0x7F];
    for (int row = 0; row < 8; row++) {
        unsigned char bits = glyph[row];
        for (int col = 0; col < 8; col++)
            fb_pixel(x + col, y + row,
                     (bits & (1 << col)) ? fg : bg);
    }
}

void gfx_putstr(const char *s, int x, int y, uint32_t fg, uint32_t bg)
{
    while (*s) {
        gfx_putfont(*s++, x, y, fg, bg);
        x += 8;
    }
}

int gfx_strlen_px(const char *s) { int n = 0; while (s[n]) n++; return n * 8; }

/* ---- day23: 绘图基元 ---- */
void fb_rect(int x, int y, int w, int h, uint32_t rgb)
{
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++)
            fb_pixel(x + i, y + j, rgb);
}

void fb_hline(int x, int y, int w, uint32_t rgb)
{
    for (int i = 0; i < w; i++)
        fb_pixel(x + i, y, rgb);
}

void fb_vline(int x, int y, int h, uint32_t rgb)
{
    for (int j = 0; j < h; j++)
        fb_pixel(x, y + j, rgb);
}

/* Bresenham 直线 */
void fb_line(int x0, int y0, int x1, int y1, uint32_t rgb)
{
    int dx = x1 - x0, dy = y1 - y0;
    int sx = dx > 0 ? 1 : -1, sy = dy > 0 ? 1 : -1;
    dx = dx > 0 ? dx : -dx;
    dy = dy > 0 ? dy : -dy;
    int err = dx - dy;
    for (;;) {
        fb_pixel(x0, y0, rgb);
        if (x0 == x1 && y0 == y1)
            break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 <  dx) { err += dx; y0 += sy; }
    }
}

/* 窗口框: 阴影 + 边框 + 标题条 */
void gfx_window(int x, int y, int w, int h, uint32_t title_rgb, const char *title)
{
    fb_rect(x + 3, y + 3, w, h, 0x00000000);          /* 投影 */
    fb_rect(x, y, w, h, 0x00E8E8E8);                  /* 窗体 */
    fb_rect(x, y, w, 16, title_rgb);                  /* 标题条 */
    fb_hline(x, y + h - 1, w, 0x00888888);
    fb_vline(x + w - 1, y, h, 0x00888888);
    if (title)
        gfx_putstr(title, x + 6, y + 4, 0x00FFFFFF, title_rgb);
}
