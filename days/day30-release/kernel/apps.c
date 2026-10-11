/* =====================================================================
 * day29 - kernel/apps.c：小应用窗口
 * about(版本信息) 与 mem(内存监视) —— 内容随 gui_refresh 刷新,
 * mem 每帧读取 mem_free_pages, 时钟走 jiffies。
 * ===================================================================== */
#include <kernel.h>
#include <string.h>

extern volatile uint32_t jiffies;
extern int cursor_x, cursor_y;

static int mem_win_id = -1;

void apps_init(void)
{
    mem_win_id = win_create(360, 250, 240, 110, 0x00602080, "memory monitor");
}

/* 由 gui_refresh 在窗口绘制后调用: 写内容区 */
void apps_render(void)
{
    if (mem_win_id < 0)
        return;
    int x, y, w, h;
    if (!term_window_rect_for(mem_win_id, &x, &y, &w, &h))
        return;
    char buf[40];
    uint32_t sec = jiffies / 100;
    strcpy(buf, "free pages: ");
    char n[12]; int i = 0, v = (int)mem_free_pages();
    if (v == 0) n[i++] = '0';
    while (v) { n[i++] = '0' + v % 10; v /= 10; }
    { int a = 0, b = i - 1; while (a < b) { char t = n[a]; n[a] = n[b]; n[b] = t; a++; b--; } }
    n[i] = 0;
    strcat(buf, n);
    gfx_putstr(buf, x + 8, y + 24, 0x00202020, 0x00E8E8E8);

    strcpy(buf, "uptime: ");
    v = (int)sec;
    i = 0;
    if (v == 0) n[i++] = '0';
    while (v) { n[i++] = '0' + v % 10; v /= 10; }
    { int a = 0, b = i - 1; while (a < b) { char t = n[a]; n[a] = n[b]; n[b] = t; a++; b--; } }
    n[i] = 0;
    strcat(buf, n);
    strcat(buf, " s");
    gfx_putstr(buf, x + 8, y + 36, 0x00202020, 0x00E8E8E8);

    strcpy(buf, "cursor: ");
    v = cursor_x; i = 0;
    while (v) { n[i++] = '0' + v % 10; v /= 10; }
    { int a = 0, b = i - 1; while (a < b) { char t = n[a]; n[a] = n[b]; n[b] = t; a++; b--; } }
    n[i] = 0;
    strcat(buf, n);
    gfx_putstr(buf, x + 8, y + 48, 0x00202020, 0x00E8E8E8);
}
