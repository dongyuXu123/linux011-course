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
    int uid;                      /* 稳定身份: z 序换位后下标失效(day30 教训,
                                     溯源回修 day26 引入换位之日起即需要) */
    int x, y, w, h;
    uint32_t title_rgb;
    const char *title;
    int visible;
};

static struct win wins[MAXWINS];
static int nwins = 0;
static int next_uid = 1;

static int idx_of_uid(int uid)
{
    for (int i = 0; i < nwins; i++)
        if (wins[i].uid == uid)
            return i;
    return -1;
}

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
    wins[nwins] = (struct win){ next_uid, x, y, w, h, rgb, title, 1 };
    nwins++;                          /* ← uid 回修时弄丢过这一行(历史重演) */
    return next_uid++;
}

void win_move(int uid, int x, int y)
{
    int i = idx_of_uid(uid);
    if (i >= 0) {
        wins[i].x = x;
        wins[i].y = y;
    }
}

struct win *win_get(int uid)
{
    int i = idx_of_uid(uid);
    return i >= 0 ? &wins[i] : &wins[0];
}

static int t_win_id = -1;
int term_win_id(void) { return t_win_id; }
void term_set_win_id(int id) { t_win_id = id; }

/* 按 id 取窗口矩形(term/apps 共用) */
int term_window_rect(int *x, int *y, int *w, int *h)
{
    int i = idx_of_uid(term_win_id());
    if (i < 0)
        return 0;
    *x = wins[i].x; *y = wins[i].y;
    *w = wins[i].w; *h = wins[i].h;
    return 1;
}

int term_window_rect_for(int id, int *x, int *y, int *w, int *h)
{
    if (id < 0 || id >= nwins)
        return 0;
    *x = wins[id].x; *y = wins[id].y;
    *w = wins[id].w; *h = wins[id].h;
    return 1;
}
int win_count(void) { return nwins; }

static void draw_cursor(int cx, int cy)
{
    for (int j = 0; j < 18; j++)
        for (int i = 0; i < 12; i++) {
            if (arrow[j][i] == 'B') fb_pixel(cx + i, cy + j, 0);
            else if (arrow[j][i] == 'W') fb_pixel(cx + i, cy + j, 0x00FFFFFF);
        }
}

/* ---- day26: 拖动 ----
 * 按下时光标命中的窗口标题条被"抓住", 移动中窗口跟随光标;
 * 抓住的窗口提到最顶(简单实现: 移到数组末尾 = 最高 z 序) */
static int drag_id = -1, drag_offx, drag_offy;
extern int mouse_buttons;

void gui_drag_step(int cx, int cy)
{
    if (mouse_buttons & 1) {                       /* 左键按住 */
        if (drag_id < 0) {
            for (int i = nwins - 1; i >= 0; i--) { /* 顶窗优先 */
                struct win *w = &wins[i];
                if (w->visible &&
                    cx >= w->x && cx < w->x + w->w &&
                    cy >= w->y && cy < w->y + 16) {
                    drag_id = i;
                    drag_offx = cx - w->x;
                    drag_offy = cy - w->y;
                    /* 提到最顶: 与数组末尾交换 */
                    struct win t = wins[i];
                    wins[i] = wins[nwins - 1];
                    wins[nwins - 1] = t;
                    drag_id = wins[nwins - 1].uid;
                    printk("[day29] window '%s' grabbed\n", wins[drag_id].title);
                    break;
                }
            }
        } else {
            win_move(drag_id, cx - drag_offx, cy - drag_offy);
        }
    } else {
        drag_id = -1;
    }
}

/* 任务栏: 底部 24px, 左侧任务按钮, 右侧时钟(jiffies→mm:ss) */
static void draw_taskbar(void)
{
    extern volatile uint32_t jiffies;
    int th = 24, w = (int)fb_width(), h = (int)fb_height();
    fb_rect(0, h - th, w, th, 0x00181830);
    fb_hline(0, h - th, w, 0x00404070);
    /* 每窗口一个任务按钮 */
    int bx = 4;
    for (int i = 0; i < nwins; i++) {
        const char *t = wins[i].title;
        int bw = 0;
        while (t[bw]) bw++;
        bw = bw * 8 + 12;
        fb_rect(bx, h - th + 3, bw, 18, 0x00303058);
        gfx_putstr(t, bx + 6, h - th + 8, 0x00E0E0FF, 0x00303058);
        bx += bw + 4;
    }
    /* 右侧时钟 */
    uint32_t sec = jiffies / 100;
    char buf[10];
    int m = (int)(sec / 60), s2 = (int)(sec % 60);
    buf[0] = '0' + m / 10; buf[1] = '0' + m % 10; buf[2] = ':';
    buf[3] = '0' + s2 / 10; buf[4] = '0' + s2 % 10; buf[5] = 0;
    gfx_putstr(buf, w - 8 * 5 - 8, h - th + 8, 0x00FFFFFF, 0x00181830);
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
    {
        void term_render(void);
        term_render();
    }
    {
        void apps_render(void);
        apps_render();
    }
    draw_taskbar();
    draw_cursor(cursor_x, cursor_y);
}
