/* =====================================================================
 * day09 - kernel/printk.c —— 内核打印(移植自 0.11 kernel/printk.c 思想)
 * 0.11 的 printk 走 tty_write 队列; 课程内核 day08 已有 con 层,
 * 这里直接 vsprintf → con_puts(串口镜像自动带上)。
 * panic() 对应 source/kernel/panic.c。
 * ===================================================================== */
#include <stdarg.h>
#include <kernel.h>

extern int vsprintf(char *buf, const char *fmt, va_list args);

int printk(const char *fmt, ...)
{
    static char buf[1024];
    va_list args;
    int i;

    va_start(args, fmt);
    i = vsprintf(buf, fmt, args);
    va_end(args);
    con_puts(buf);
    return i;
}

void panic(const char *fmt, ...)
{
    static char buf[512];
    va_list args;

    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);
    printk("\nKernel panic: %s\n", buf);
    for (;;)
        __asm__ volatile("cli; hlt");
}
