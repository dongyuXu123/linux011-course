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
    int uid;                      /* 稳定身份: z 序交换不影响外部引用 */
    int x, y, w, h;
    uint32_t title_rgb;
    char     tbuf[16];            /* 标题自持拷贝(防调用方缓冲被改写) */
    const char *title;            /* 恒指向本结构内 tbuf */
    int visible;
};

static struct win wins[MAXWINS];
static int nwins = 0;
static int next_uid = 1;
static int focused = -1;          /* 活动窗口的 uid(标题条/任务按钮高亮, 键盘去处) */

int gui_focused(void) { return focused; }

void gui_set_focus(int uid)
{
    focused = uid;
}

static int idx_of_uid(int uid)
{
    for (int i = 0; i < nwins; i++)
        if (wins[i].uid == uid)
            return i;
    return -1;
}

static void wins_repair_titles(void);

/* 把窗口提到数组末尾(最高 z 序)并设为焦点; 返回是否真的改变了层序 */
static int raise_to_top(int i)
{
    if (i < 0)
        return 0;
    int changed = (i != nwins - 1);
    if (changed) {
        struct win t = wins[i];
        wins[i] = wins[nwins - 1];
        wins[nwins - 1] = t;
        wins_repair_titles();
    }
    focused = wins[nwins - 1].uid;   /* 焦点按 uid, 索引换位无影响 */
    return changed;
}

static int topmost_win_at(int cx, int cy)
{
    for (int i = nwins - 1; i >= 0; i--) {
        struct win *w = &wins[i];
        if (w->visible &&
            cx >= w->x && cx < w->x + w->w &&
            cy >= w->y && cy < w->y + w->h)
            return i;
    }
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
    /* day39: 标题必须自持拷贝 —— 系统调用侧传来的指针指向共享
       kbuf, 后续任何系统调用都会改写它(曾致所有窗口同名) */
    wins[nwins] = (struct win){ .uid = next_uid, .x = x, .y = y,
        .w = w, .h = h, .title_rgb = rgb, .visible = 1 };
    {
        int k = 0;
        for (; k < 15 && title[k]; k++)
            wins[nwins].tbuf[k] = title[k];
        wins[nwins].tbuf[k] = 0;
        wins[nwins].title = wins[nwins].tbuf;
    }
    nwins++;                          /* ← uid 重写时弄丢过这一行 */
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

/* wins[] 任意结构体拷贝后调用: title 恒指回自身 tbuf */
static void wins_repair_titles(void)
{
    for (int i = 0; i < nwins; i++)
        wins[i].title = wins[i].tbuf;
}

static int t_win_id = -1;
int term_win_id(void) { return t_win_id; }
void term_set_win_id(int id) { t_win_id = id; }

/* 按 uid 取窗口矩形(term/apps 共用) */
int term_window_rect(int *x, int *y, int *w, int *h)
{
    int i = idx_of_uid(term_win_id());
    if (i < 0)
        return 0;
    *x = wins[i].x; *y = wins[i].y;
    *w = wins[i].w; *h = wins[i].h;
    return 1;
}

int term_window_rect_for(int uid, int *x, int *y, int *w, int *h)
{
    int i = idx_of_uid(uid);
    if (i < 0)
        return 0;
    *x = wins[i].x; *y = wins[i].y;
    *w = wins[i].w; *h = wins[i].h;
    return 1;
}
int win_count(void) { return nwins; }

/* 系统调用按 uid 取窗口位置 —— uid 是窗口唯一身份(day30 教训:
   z 序换位后数组下标会失效, 一律扫描 .uid 字段) */
int win_rect_xy(int id, int *x, int *y)
{
    for (int i = 0; i < nwins; i++)
        if (wins[i].uid == id) {
            *x = wins[i].x;
            *y = wins[i].y;
            return 1;
        }
    return 0;
}
int win_uid_at(int i) { return (i >= 0 && i < nwins) ? wins[i].uid : -1; }

static void draw_cursor(int cx, int cy)
{
    for (int j = 0; j < 18; j++)
        for (int i = 0; i < 12; i++) {
            if (arrow[j][i] == 'B') fb_pixel(cx + i, cy + j, 0);
            else if (arrow[j][i] == 'W') fb_pixel(cx + i, cy + j, 0x00FFFFFF);
        }
}

/* ---- day26/day30: 点击选中 + 标题条拖动 ----
 * 按下沿: 命中的窗口(任意位置)置顶并成为活动窗口;
 *         若按在标题条上则同时"抓住"进入拖动。 */
static int drag_id = -1, drag_offx, drag_offy;
extern int mouse_buttons;

extern int mouse_take_press_edge(void);

int gui_pointer_step(int cx, int cy)
{
    int press = mouse_take_press_edge();           /* 包级按下沿, 不丢 */
    int btn = mouse_buttons & 1;
    int changed = 0;
    if (press) {                                   /* 按下沿 */
        int hit = topmost_win_at(cx, cy);
        printk("[hit] hit=%d nwins=%d\n", hit, nwins);
        if (hit >= 0) {
            changed = raise_to_top(hit);
            printk("[day30] focus -> %s\n", wins[nwins - 1].title);
            if (cy - wins[nwins - 1].y < 16) {     /* 按在标题条上: 抓住 */
                drag_id = wins[nwins - 1].uid;
                drag_offx = cx - wins[nwins - 1].x;
                drag_offy = cy - wins[nwins - 1].y;
                printk("[day30] window '%s' grabbed\n", wins[nwins - 1].title);
            }
        }
    } else if (btn && drag_id >= 0) {              /* 拖动中 */
        win_move(drag_id, cx - drag_offx, cy - drag_offy);
    }
    if (!btn)
        drag_id = -1;
    return changed;
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
        uint32_t bg = (wins[i].uid == focused) ? 0x00505088 : 0x00303058;
        fb_rect(bx, h - th + 3, bw, 18, bg);
        gfx_putstr(t, bx + 6, h - th + 8, 0x00E0E0FF, bg);
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

/* day39: 焦点轮转(Alt-Tab 语义): 焦点移到下一窗并置顶。
   day30 教训延续: 换位后 uid 身份不变, t_win_id 照样认得窗口 */
void gui_focus_next(void)
{
    if (nwins < 2)
        return;
    int cur = 0;
    for (int i = 0; i < nwins; i++)
        if (wins[i].uid == t_win_id)
            cur = i;
    struct win t = wins[cur];
    for (int i = cur; i < nwins - 1; i++)
        wins[i] = wins[i + 1];      /* cur 挪到末尾, 后继成为新的 cur */
    wins[nwins - 1] = t;
    wins_repair_titles();           /* 结构体拷贝会带走指向旧槽的 title */
    gui_set_focus(wins[cur].uid);   /* 原后继(现在在 cur 位)拿焦点 */
    printk("[day39] focus -> %s\n", wins[cur].title);
}

/* z 序自底向上整屏重绘: 先画进内存缓冲, 再一次上屏(双缓冲) */
static uint32_t shadow[640 * 480];

void gui_refresh(int cursor_x, int cursor_y)
{
    fb_set_target(shadow);

    /* 背景: 渐变 + 底纹格子 */
    for (int y = 0; y < (int)fb_height(); y++)
        for (int x = 0; x < (int)fb_width(); x++) {
            uint32_t c = ((x / 3) << 16) | ((y / 3) << 8) | 0x402030;
            if (((x / 32) + (y / 32)) % 2 == 0)
                c = ((x / 3) >> 1 << 16) | ((y / 3) >> 1 << 8) | 0x302838;
            fb_pixel(x, y, c);
        }
    for (int i = 0; i < nwins; i++) {
        if (!wins[i].visible)
            continue;
        /* 活动窗口标题用原色, 非活动变暗 */
        uint32_t t = (wins[i].uid == focused)
            ? wins[i].title_rgb
            : ((wins[i].title_rgb >> 1) & 0x7F7F7F);
        gfx_window(wins[i].x, wins[i].y, wins[i].w, wins[i].h,
                   t, wins[i].title);
    }
    {
        void term_render(void);
        term_render();
    }
    draw_taskbar();
    draw_cursor(cursor_x, cursor_y);

    fb_set_target(0);
    fb_flush(shadow);
}
