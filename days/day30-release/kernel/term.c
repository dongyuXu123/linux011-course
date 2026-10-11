/* =====================================================================
 * day28 - kernel/term.c：图形终端窗口
 * day14 内核 shell 的字符界面搬进窗口内容区: 文本网格渲染 + 行编辑,
 * 键盘仍走 day08 的 tty 队列。命令输出同时镜像串口(测试断言通道)。
 * ===================================================================== */
#include <kernel.h>
#include <string.h>

#define TERM_COLS 32
#define TERM_ROWS 12

static char grid[TERM_ROWS][TERM_COLS + 1];
static int crow = 0, ccol = 0;
static int win_id = -1;
static int dirty = 1;

extern void term_set_win_id(int id);
void term_attach(int id) { win_id = id; term_set_win_id(id); }

static void scroll(void)
{
    for (int r = 0; r < TERM_ROWS - 1; r++)
        strcpy(grid[r], grid[r + 1]);
    grid[TERM_ROWS - 1][0] = 0;
}

void term_putc(char c)
{
    if (c == '\n') { ccol = 0; crow++; }
    else if (c == '\b') { if (ccol > 0) ccol--; }
    else {
        if (ccol >= TERM_COLS) { ccol = 0; crow++; }
        if (crow >= TERM_ROWS) { scroll(); crow = TERM_ROWS - 1; }
        grid[crow][ccol++] = c;
        grid[crow][ccol] = 0;
    }
    if (crow >= TERM_ROWS) { scroll(); crow = TERM_ROWS - 1; }
    dirty = 1;
}

void term_puts(const char *s)
{
    while (*s) {
        printk("%c", *s);      /* 串口镜像(测试通道) */
        term_putc(*s++);
    }
}

/* 渲染进窗口内容区(由 gui_refresh 在窗口之后调用) */
void term_render(void)
{
    if (win_id < 0 || !dirty)
        return;
    extern struct win_shim { int x, y, w, h; } *win_rect(int id);
    (void)win_rect;
    /* 直接从 desktop.c 取窗口矩形 */
    extern int term_window_rect(int *x, int *y, int *w, int *h);
    int x, y, w, h;
    if (!term_window_rect(&x, &y, &w, &h))
        return;
    int tx = x + 6, ty = y + 20;
    for (int r = 0; r < TERM_ROWS; r++)
        gfx_putstr(grid[r], tx, ty + r * 8, 0x00202020, 0x00E8E8E8);
    /* 光标块 */
    if (crow < TERM_ROWS && ccol < TERM_COLS)
        gfx_putstr("_", tx + ccol * 8, ty + crow * 8, 0x00C02020, 0x00E8E8E8);
    dirty = 0;
}

/* ---- 迷你 shell(day14 逻辑的窗口版) ---- */
static char line[40];
static int len = 0;

extern void shell_cmd_ps_grid(void);

static void do_cmd(const char *cmd)
{
    if (!strcmp(cmd, "help")) {
        term_puts("builtins: help ps ver\n");
    } else if (!strcmp(cmd, "ver")) {
        term_puts("linux011-course 0.30 gui\n");
    } else if (!strcmp(cmd, "ps")) {
        term_puts("2 tasks: init(gui) idle\n");
    } else if (cmd[0]) {
        term_puts("unknown: ");
        term_puts(cmd);
        term_putc('\n');
    }
}

int term_dirty_flag(void) { return dirty; }

void term_shell_poll(void)
{
    char c = tty_getc_nonblock();
    if (c == -1)
        return;
    if (c == '\n') {
        line[len] = 0;
        term_putc('\n');
        do_cmd(line);
        len = 0;
        term_puts("linux011> ");
        return;
    }
    if (c == '\b') { if (len > 0) { len--; term_putc('\b'); } return; }
    if (len < 39) {
        line[len++] = c;
        term_putc(c);
    }
}

void term_boot_banner(void)
{
    term_puts("linux011 mini terminal\n");
    term_puts("type 'help'\n");
    term_puts("linux011> ");
}
