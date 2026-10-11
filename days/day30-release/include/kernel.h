/* =====================================================================
 * kernel.h —— 课程内核公共声明(随课程逐课生长)
 * 对照: source/include/linux/sched.h 等 0.11 头文件
 * ===================================================================== */
#ifndef _KERNEL_H
#define _KERNEL_H

#include <stdint.h>

/* ---- 中断现场(压栈内存序: 段寄存器最后压=地址最低) ---- */
struct pt_regs {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, oesp;
    uint32_t ebx, edx, ecx, eax;
    uint32_t vector, err;
    uint32_t eip, cs, eflags;
};

/* ---- console.c ---- */
void con_init(void);
void con_putc(char c);
void con_puts(const char *s);
void con_puthex(uint32_t v);
void con_clear(void);

/* ---- traps.c ---- */
void trap_init(void);
void trap_install_gate(int vec, uint32_t handler);
void trap_dispatch(struct pt_regs *regs);

/* ---- irq.c ---- */
typedef void (*irq_handler_t)(void *regs);
void irq_init(void);
void register_irq(int irq, irq_handler_t fn);

/* ---- printk.c ---- */
int printk(const char *fmt, ...);
void panic(const char *fmt, ...);

/* ---- mm.c ---- */
void mem_init(uint32_t start, uint32_t upper_kb);
uint32_t get_free_page(void);
void free_page(uint32_t addr);
uint32_t mem_free_pages(void);
extern const uint32_t saved_mbi, saved_magic;

/* ---- sched.c: 任务模型(day11 起逐课生长) ---- */
#define NR_TASKS   4
struct task_struct {
    volatile long state;        /* 0 runnable, 1 zombie, 2 dead */
    long counter;
    long priority;
    int  pid;
    int  exit_code;
    char name;
    uint32_t kesp;
    uint32_t kstack_top;    /* 本任务内核栈顶(do_fork 复制基准) */
};
enum { TASK_RUNNABLE = 0, TASK_ZOMBIE = 1, TASK_DEAD = 2 };
extern struct task_struct *current;
extern volatile uint32_t jiffies;
extern struct task_struct task[NR_TASKS];

/* ---- sched.c (day12) ---- */
void sched_init(void);
void kernel_task_create(int n, void (*fn)(void), char name);
void schedule(void);
void do_timer(void);
int  fork(void);
void exit(int code) __attribute__((noreturn));
int  wait(int *status);

/* ---- fb.c ---- */
int fb_init(void);
int fb_is_active(void);
uint32_t fb_width(void);
uint32_t fb_height(void);
void fb_pixel(int x, int y, uint32_t rgb);
void fb_fill(uint32_t rgb);
void fb_set_target(uint32_t *buf);
void fb_flush(const uint32_t *src);
void con_serial_only_set(void);

/* ---- gfx.c ---- */
void gfx_putfont(char ch, int x, int y, uint32_t fg, uint32_t bg);
void gfx_putstr(const char *s, int x, int y, uint32_t fg, uint32_t bg);
void fb_rect(int x, int y, int w, int h, uint32_t rgb);
void fb_hline(int x, int y, int w, uint32_t rgb);
void fb_vline(int x, int y, int h, uint32_t rgb);
void fb_line(int x0, int y0, int x1, int y1, uint32_t rgb);
void gfx_window(int x, int y, int w, int h, uint32_t title_rgb, const char *title);

/* ---- desktop.c ---- */
int  win_create(int x, int y, int w, int h, uint32_t rgb, const char *title);
void win_move(int id, int x, int y);
void gui_refresh(int cursor_x, int cursor_y);
int  gui_pointer_step(int cx, int cy);   /* 返回1: 焦点/层序变化 */
int  gui_focused(void);
void gui_set_focus(int id);

/* ---- term.c ---- */
void term_attach(int id);
void term_render(void);
int  term_window_rect(int *x, int *y, int *w, int *h);
int  win_uid_at(int i);
void term_shell_poll(void);
void term_blink(void);
void term_boot_banner(void);
int term_dirty_flag(void);
char tty_getc_nonblock(void);

/* ---- apps.c ---- */
void apps_init(void);
void apps_render(void);
int term_window_rect_for(int id, int *x, int *y, int *w, int *h);
uint32_t mem_free_pages(void);

/* ---- mouse.c ---- */
void mouse_init(void);
void mouse_task_update(void);
uint32_t fb_getpixel(int x, int y);
extern int cursor_x, cursor_y;
extern int mouse_buttons;

/* ---- syscall.c ---- */
struct idt_entry {
    uint16_t off_lo;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  type_attr;
    uint16_t off_hi;
} __attribute__((packed));
void syscall_init(void);

/* ---- timer.c ---- */
void sched_init(void);
void kernel_task_create(int n, void (*fn)(void), char name);
void schedule(void);
void do_timer(void);

/* ---- fb.c ---- */
int fb_init(void);
int fb_is_active(void);
uint32_t fb_width(void);
uint32_t fb_height(void);
void fb_pixel(int x, int y, uint32_t rgb);
void fb_fill(uint32_t rgb);
void fb_set_target(uint32_t *buf);
void fb_flush(const uint32_t *src);
void con_serial_only_set(void);

/* ---- gfx.c ---- */
void gfx_putfont(char ch, int x, int y, uint32_t fg, uint32_t bg);
void gfx_putstr(const char *s, int x, int y, uint32_t fg, uint32_t bg);
void fb_rect(int x, int y, int w, int h, uint32_t rgb);
void fb_hline(int x, int y, int w, uint32_t rgb);
void fb_vline(int x, int y, int h, uint32_t rgb);
void fb_line(int x0, int y0, int x1, int y1, uint32_t rgb);
void gfx_window(int x, int y, int w, int h, uint32_t title_rgb, const char *title);

/* ---- desktop.c ---- */
int  win_create(int x, int y, int w, int h, uint32_t rgb, const char *title);
void win_move(int id, int x, int y);
void gui_refresh(int cursor_x, int cursor_y);
int  gui_pointer_step(int cx, int cy);   /* 返回1: 焦点/层序变化 */
int  gui_focused(void);
void gui_set_focus(int id);

/* ---- term.c ---- */
void term_attach(int id);
void term_render(void);
int  term_window_rect(int *x, int *y, int *w, int *h);
int  win_uid_at(int i);
void term_shell_poll(void);
void term_blink(void);
void term_boot_banner(void);
int term_dirty_flag(void);
char tty_getc_nonblock(void);

/* ---- apps.c ---- */
void apps_init(void);
void apps_render(void);
int term_window_rect_for(int id, int *x, int *y, int *w, int *h);
uint32_t mem_free_pages(void);

/* ---- mouse.c ---- */
void mouse_init(void);
void mouse_task_update(void);
uint32_t fb_getpixel(int x, int y);
extern int cursor_x, cursor_y;
extern int mouse_buttons;

/* ---- syscall.c ---- */
struct idt_entry {
    uint16_t off_lo;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  type_attr;
    uint16_t off_hi;
} __attribute__((packed));
void syscall_init(void);

/* ---- timer.c ---- */
void timer_init(void);
extern volatile uint32_t jiffies;

#endif /* _KERNEL_H */
int strncmp(const char *a, const char *b, unsigned n);
char *strchr(const char *s, int c);
