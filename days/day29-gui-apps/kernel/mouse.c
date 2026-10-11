/* =====================================================================
 * day24 - kernel/mouse.c：PS/2 鼠标(轮询式) + 光标精灵
 * ---------------------------------------------------------------------
 * 8042 辅助通道: 0x64 写 0xA8 开启; 0xD4 包装鼠标命令(0xF6 默认,
 * 0xF4 开始上报)。数据经 0x60 读出, 状态口 0x64 的 bit5 标明"来自
 * 鼠标", bit0 标明"有数据" —— 课程用主循环轮询而非 IRQ(0.11 的
 * 软盘驱动也是轮询风格; 对照 source/kernel/chr_drv/keyboard.S)。
 * 数据包 3 字节: [状态][dx][dy]; 状态 bit4/5 = x/y 符号, bit0/1 = 键。
 * 光标: 保存/恢复光标下背景块后画精灵, 不必整屏重绘。
 * ===================================================================== */
#include <kernel.h>

static inline uint8_t inb_(uint16_t p) { uint8_t r; __asm__ volatile("inb %1,%0":"=a"(r):"Nd"(p)); return r; }
static inline void outb_(uint16_t p, uint8_t v) { __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p)); }

static void mouse_wait_write(void) { for (int i = 0; i < 100000 && (inb_(0x64) & 2); i++); }
static void mouse_wait_read(void)  { for (int i = 0; i < 100000 && !(inb_(0x64) & 1); i++); }

static void mouse_cmd(uint8_t c)
{
    mouse_wait_write(); outb_(0x64, 0xD4);
    mouse_wait_write(); outb_(0x60, c);
    mouse_wait_read();
    inb_(0x60);                        /* 丢弃 ACK */
}

static uint8_t pkt[3];
static int mouse_cycle = 0;
static int mouse_dx_acc, mouse_dy_acc, mouse_btn;
static int packets = 0;

int cursor_x = 320, cursor_y = 240;
int mouse_buttons = 0;

void mouse_init(void)
{
    mouse_wait_write(); outb_(0x64, 0xA8);      /* 启用辅助通道 */
    mouse_wait_write(); outb_(0x64, 0x20);
    mouse_wait_read();
    uint8_t cmd = inb_(0x60);
    mouse_wait_write(); outb_(0x64, 0x60);
    mouse_wait_write(); outb_(0x60, cmd | 2);   /* 打开鼠标时钟/中断位 */
    mouse_cmd(0xF6);                            /* 默认设置 */
    mouse_cmd(0xF4);                            /* 开始上报 */
    while (inb_(0x64) & 1)                      /* 排空残余 */
        inb_(0x60);
    mouse_cycle = 0;
}

/* 主循环反复调用: 从 8042 收字节, 组满 3 字节包则累积位移 */
void mouse_task_update(void)
{
    uint8_t st = inb_(0x64);
    if (!(st & 1))
        return;                        /* 无数据 */
    uint8_t d = inb_(0x60);
    if (!(st & 0x20))
        return;                        /* 键盘数据, 不归我们 */
    pkt[mouse_cycle++] = d;
    if (mouse_cycle < 3)
        return;
    mouse_cycle = 0;
    if (pkt[0] & 0xC0)
        return;                        /* 溢出丢弃 */
    int dx = pkt[1], dy = pkt[2];
    if (pkt[0] & 0x10) dx -= 256;
    if (pkt[0] & 0x20) dy -= 256;
    mouse_dx_acc += dx;
    mouse_dy_acc -= dy;                /* 屏幕 y 向下 */
    mouse_btn = pkt[0] & 3;
    mouse_buttons = mouse_btn;
    packets++;
    if (dx || dy || mouse_btn) {
        cursor_x += mouse_dx_acc;
        cursor_y += mouse_dy_acc;
        mouse_dx_acc = mouse_dy_acc = 0;
        if (cursor_x < 0) cursor_x = 0;
        if (cursor_y < 0) cursor_y = 0;
        if (cursor_x > 627) cursor_x = 627;
        if (cursor_y > 461) cursor_y = 461;
        static int printed = 0;
        if (printed++ < 5)
            printk("[day29] cursor at (%d,%d) btn=%d\n",
                   cursor_x, cursor_y, mouse_btn);
    }
}
