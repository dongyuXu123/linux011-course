/* =====================================================================
 * day21 - kernel/fb.c：线性帧缓冲(VBE/Dispi) —— 图形时代的开始
 * ---------------------------------------------------------------------
 * 模式设置: QEMU/Bochs 的 VBE Dispi 接口(端口 0x1CE/0x1CF)允许在
 * 保护模式直接设 640x480x32(真硬件上这步由实模式 VBE BIOS 完成,
 * GRUB2 对 multiboot1 的 gfxpayload 在本环境也不切模式, 故自力更生)。
 * LFB 物理基址: QEMU 把 VGA PCI BAR 分配在 0xFD000000 —— 以 GRUB
 * 传来的 multiboot_info framebuffer 字段(flags bit12)为准。
 * LFB 在恒等映射(0~16MB)之外, 需给内核页表追加 2 页表映射。
 * ===================================================================== */
#include <kernel.h>

#define DISPI_INDEX  0x1CE
#define DISPI_DATA   0x1CF
#define DISPI_ID          0x00
#define DISPI_XRES        0x01
#define DISPI_YRES        0x02
#define DISPI_BPP         0x03
#define DISPI_ENABLE      0x04
#define DISPI_VIRT_WIDTH  0x06
#define DISPI_X_OFFSET    0x08
#define DISPI_Y_OFFSET    0x09
#define DISPI_ENABLED     0x01
#define DISPI_LFB         0x40
#define BOCHS_LFB_ADDR    0xE0000000   /* Bochs 文档值, QEMU 实际见下 */

struct mb_fb {
    uint32_t flags;
    uint32_t mem_lower, mem_upper;
    uint32_t boot_device, cmdline;
    uint32_t mods_count, mods_addr;
    uint32_t syms[5];
    uint32_t mmap_len, mmap_addr;
    uint32_t drives_len, drives_addr;
    uint32_t config_table, boot_loader_name;
    uint32_t apm_table;
    uint32_t vbe[3];           /* GRUB2 实测: 仅 3 项, fb 块从 +88 开始 */
    uint32_t fb_addr_lo, fb_addr_hi;
    uint32_t fb_pitch;
    uint32_t fb_width, fb_height;
    uint8_t  fb_bpp, fb_type;
};

extern const uint32_t saved_mbi;
extern uint32_t page_dir;

static volatile uint32_t *lfb;
static uint32_t fb_w, fb_h, fb_pitch;
static int fb_active = 0;

static uint32_t pgtable_fb[1024] __attribute__((aligned(4096)));
static uint32_t pgtable_fb2[1024] __attribute__((aligned(4096)));

static inline void outw_(uint16_t p, uint16_t v) { __asm__ volatile("outw %0,%1"::"a"(v),"Nd"(p)); }

static void dispi_set(uint16_t idx, uint16_t val)
{
    outw_(DISPI_INDEX, idx);
    outw_(DISPI_DATA, val);
}

int fb_init(void)
{
    dispi_set(DISPI_ENABLE, 0);
    dispi_set(DISPI_ID, 0xB0C5);
    dispi_set(DISPI_XRES, 640);
    dispi_set(DISPI_YRES, 480);
    dispi_set(DISPI_BPP, 32);
    dispi_set(DISPI_VIRT_WIDTH, 640);
    dispi_set(DISPI_X_OFFSET, 0);
    dispi_set(DISPI_Y_OFFSET, 0);
    dispi_set(DISPI_ENABLE, DISPI_ENABLED | DISPI_LFB);

    /* LFB 基址: 直接探 PCI 配置空间 —— VGA(总线0/设备2/功能0) 的
       BAR0 就是线性帧缓冲(0.11 没有这段; 现代内核 pci.c 的雏形)。
       GRUB 的 gfxpayload 对 multiboot1 不可靠(可能上报文本 0xB8000) */
    struct mb_fb *m = (struct mb_fb *)saved_mbi;
    uint32_t base = 0, bar = 0;
    {
        __asm__ volatile("outl %0, %1" : : "a"(0x80004010u), "Nd"((uint16_t)0xCF8));
        __asm__ volatile("inl %1, %0" : "=a"(bar) : "Nd"((uint16_t)0xCFC));
        if (bar != 0xFFFFFFFFu && bar != 0 && (bar & 1u) == 0)
            base = bar & 0xFFFFFFF0u;      /* 32 位 MMIO BAR */
    }
    if (base < 0x1000000u) {               /* 探测失败: 依次退回 */
        base = 0;
        if ((m->flags & (1 << 12)) && m->fb_addr_lo > 0x1000000u)
            base = m->fb_addr_lo;          /* GRUB 报告的图形 LFB */
        if (!base)
            base = 0xFD000000u;            /* QEMU Bochs VGA 实测值 */
        printk("[day37] pci bar probe: bar=%x, fallback base=%x\n",
               bar, base);
    }

    /* 页表映射: LFB 区 2 页表(8MB 覆盖), 供内核直写 */
    uint32_t pde = base >> 22;
    for (int i = 0; i < 1024; i++)
        pgtable_fb[i] = (base & ~0x3FFFFF) + (i << 12) | 0x3;
    for (int i = 0; i < 1024; i++)
        pgtable_fb2[i] = (base & ~0x3FFFFF) + 0x400000 + (i << 12) | 0x3;
    ((volatile uint32_t *)&page_dir)[pde] = (uint32_t)pgtable_fb | 0x3;
    ((volatile uint32_t *)&page_dir)[pde + 1] = (uint32_t)pgtable_fb2 | 0x3;

    lfb = (volatile uint32_t *)base;
    fb_w = 640;
    fb_h = 480;
    fb_pitch = 640 * 4;
    fb_active = 1;
    printk("[day30] framebuffer %dx%d pitch=%d at %x (mapped)\n",
           fb_w, fb_h, fb_pitch, base);
    return 1;
}

int fb_is_active(void) { return fb_active; }
uint32_t fb_width(void)  { return fb_w; }
uint32_t fb_height(void) { return fb_h; }

/* 渲染目标: 0=LFB 直写; 指向内存缓冲时, gui_refresh 先画到缓冲,
   再用 fb_flush 一次性上屏 —— 消除逐像素直写的撕裂/闪烁 */
static volatile uint32_t *render_target = 0;
volatile uint32_t *render_target_get(void) { return render_target; }

void fb_set_target(uint32_t *buf)
{
    render_target = buf;
}

void fb_pixel(int x, int y, uint32_t rgb)
{
    if (!fb_active || x < 0 || y < 0 || x >= (int)fb_w || y >= (int)fb_h)
        return;
    volatile uint32_t *dst = render_target ? render_target : lfb;
    dst[y * (fb_pitch / 4) + x] = rgb;
}

/* 把一帧缓冲按行拷到 LFB(双缓冲上屏) */
void fb_flush(const uint32_t *src)
{
    if (!fb_active)
        return;
    int stride = fb_pitch / 4;
    for (uint32_t y = 0; y < fb_h; y++) {
        volatile uint32_t *d = lfb + y * stride;
        const uint32_t *s = src + y * fb_w;
        for (uint32_t x = 0; x < fb_w; x++)
            d[x] = s[x];
    }
}

/* 上屏当前渲染目标(day37 系统调用侧用) */
void fb_flush_current(void)
{
    extern volatile uint32_t *render_target_get(void);
    volatile uint32_t *t = render_target_get();
    if (t)
        fb_flush((const uint32_t *)t);
}

void fb_fill(uint32_t rgb)
{
    for (uint32_t y = 0; y < fb_h; y++)
        for (uint32_t x = 0; x < fb_w; x++)
            fb_pixel(x, y, rgb);
}

uint32_t fb_getpixel(int x, int y)
{
    if (x < 0 || y < 0 || x >= (int)fb_w || y >= (int)fb_h)
        return 0;
    return lfb[y * (fb_pitch / 4) + x];
}
