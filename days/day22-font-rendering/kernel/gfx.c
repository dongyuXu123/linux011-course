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
