/* =====================================================================
 * day30 - kernel/splash.c：启动画面
 * 深色底 + 课程名 + 30 天进度条动画(纯视觉, ~2 秒), 之后进桌面。
 * ===================================================================== */
#include <kernel.h>

extern volatile uint32_t jiffies;

void splash_show(void)
{
    fb_fill(0x00101418);
    /* 标题条 */
    fb_rect(140, 190, 360, 60, 0x00182030);
    gfx_putstr("linux011-course", 226, 200, 0x00FFFFFF, 0x00182030);
    gfx_putstr("30 days from MBR to desktop", 190, 216, 0x0088CCFF, 0x00182030);
    /* 进度条: 30 格 = 30 天 */
    for (int d = 0; d < 30; d++) {
        fb_rect(146 + d * 12, 268, 8, 12, 0x0030A040);
        uint32_t t0 = jiffies;
        while (jiffies - t0 < 6)
            __asm__ volatile("hlt");
    }
    gfx_putstr("v0.30 (c) the course, GRUB booted", 180, 300, 0x00666666, 0x00101418);
    uint32_t t0 = jiffies;
    while (jiffies - t0 < 100)
        __asm__ volatile("hlt");
    printk("[day30] splash shown, entering desktop\n");
}
