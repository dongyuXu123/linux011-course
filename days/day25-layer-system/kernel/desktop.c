/* =====================================================================
 * day25 - kernel/desktop.c：图层系统
 * ---------------------------------------------------------------------
 * 屏幕由图层叠出: 背景(0) → 各窗口(1..n) → 光标(最顶)。
 * 任何变化后 gui_refresh() 自底向上重绘整个 z 序 —— 30dayMakeOS
 * day10 的 sheet 系统思想; 0.11 没有图形栈, 这是课程化扩展。
 * ===================================================================== */
#include <kernel.h>

#define MAXWINS 4

struct win {
    int x, y, w, h;
    uint32_t title_rgb;
    const char *title;
    int visible;
};

static struct win wins[MAXWINS];
static int nwins = 0;

/* 光标精灵(与 day24 相同的 12x18 箭头) */
static const char *arrow[18] = {
    "B...........", "BB..........", "BWB.........", "BWWB........",
    "BWWWB.......", "BWWWWB......", "BWWWWWB.....", "BWWWWWWB....",
    "BWWWWWWWB...", "BWWWWWWWWB..", "BWWWWWBBBB..", "BWWBWWB.....",
    "BWB.BWWB....", "BB..BWWB....", "B....BWWB...", " .....BWWB..",
    ".......BWWB.", ".......BBB..",
};

int win_create(int x, int y, int w, int h, uint32_t rgb, const char *title)
{
    if (nwins >= MAXWINS)
        return -1;
    wins[nwins] = (struct win){ x, y, w, h, rgb, title, 1 };
    return nwins++;
}

void win_move(int id, int x, int y)
{
    if (id >= 0 && id < nwins) {
        wins[id].x = x;
        wins[id].y = y;
    }
}

struct win *win_get(int id) { return &wins[id]; }
int win_count(void) { return nwins; }

static void draw_cursor(int cx, int cy)
{
    for (int j = 0; j < 18; j++)
        for (int i = 0; i < 12; i++) {
            if (arrow[j][i] == 'B') fb_pixel(cx + i, cy + j, 0);
            else if (arrow[j][i] == 'W') fb_pixel(cx + i, cy + j, 0x00FFFFFF);
        }
}

/* z 序自底向上整屏重绘 */
void gui_refresh(int cursor_x, int cursor_y)
{
    /* 背景: 渐变 + 底纹格子 */
    for (int y = 0; y < (int)fb_height(); y++)
        for (int x = 0; x < (int)fb_width(); x++) {
            uint32_t c = ((x / 3) << 16) | ((y / 3) << 8) | 0x402030;
            if (((x / 32) + (y / 32)) % 2 == 0)
                c = ((x / 3) >> 1 << 16) | ((y / 3) >> 1 << 8) | 0x302838;
            fb_pixel(x, y, c);
        }
    for (int i = 0; i < nwins; i++)
        if (wins[i].visible)
            gfx_window(wins[i].x, wins[i].y, wins[i].w, wins[i].h,
                       wins[i].title_rgb, wins[i].title);
    draw_cursor(cursor_x, cursor_y);
}
