# day21 - framebuffer-vbe：线性帧缓冲 640×480×32，图形时代的开始

离开字符终端。`kernel/fb.c` 新增图形基座：通过 QEMU/Bochs 的 **VBE Dispi 接口**（索引口 `0x1CE`、数据口 `0x1CF`）在保护模式直接把显示模式设为 640×480×32 的线性帧缓冲（LFB）；再给内核页表**追加两张页表**把 LFB 物理区映射进来（它远在 day06 恒等映射的 16MB 之外）。`kmain` 初始化后先 `con_serial_only_set()` 让文本控制台退场，然后画一幅 R 随 x、G 随 y 的渐变背景，中央叠一块橙色矩形，`hlt` 常驻供测试器截图。

本环境的关键事实：GRUB2 2.14 对 multiboot1 的 `gfxpayload` 不起作用、`module` 又报 out of memory（day19 已因此改道 ATA），所以**设模式这一步彻底自力更生**用 Dispi 端口完成；LFB 物理基址则取 GRUB multiboot_info framebuffer 字段（flags bit12）报告的 **PCI BAR 值 0xFD000000**（QEMU 的分配结果；若字段缺失则退回 Bochs 文档值 0xE0000000）。

串口输出：

```
[day21] framebuffer 640x480 pitch=2560 at fd000000 (mapped)
[day21] gradient painted on LFB
[day21] orange rect at (240,200)-(400,280)
```

## 知识点

- **VBE Dispi 端口编程**：写法是"先写索引再写数据"——`outw(0x1CE, idx); outw(0x1CF, val)`。序列：ENABLE=0 关显示 → ID=0xB0C5（QEMU 支持的版本号）→ XRES/YRES/BPP → VIRT_WIDTH → ENABLE=0x01|0x40（使能 + LFB 标志，不置 LFB 位则是老式 banked 模式）。真硬件上这一步要靠实模式 VBE BIOS（int 10h AX=4F02h），保护模式无法直接调——课程借助 QEMU 的 Bochs 风格接口绕开。
- **LFB 基址的来源**：像素写进的是物理内存而不是 I/O 口。QEMU 把 VGA 卡的 PCI BAR 分配在 `0xFD000000`，GRUB（虽然没能切模式）仍把模式信息块填在 multiboot_info 里——`flags bit12` 有效时 `fb_addr_lo` 就是权威值。**优先信引导器、其次信文档常量**是驱动开发的正确姿势。
- **页表热扩展**：day06 的页目录只映射了 0~16MB。`fb_init` 直接改 `page_dir[pde]`/`page_dir[pde+1]` 指向两张新页表（每张 1024 项、项 i = 基址+i×4KB | 0x3），合计覆盖 8MB LFB 区——内核（ring0）从此能像普通内存一样写显存。改页目录后无需 INVLPG（新映射此前未用过）。
- **pitch 与像素寻址**：pitch = 640×4 = **2560 字节/行**（32bpp 每像素 4 字节），`fb_pixel(x,y)` 落在 `lfb[y*(pitch/4) + x]`。格式按 0x00RRGGBB（little-endian 下字节序 B,G,R,x）写入。
- **文本控制台的退场**：图形模式下 VGA 文本缓冲不再可见，`con_serial_only_set()` 让 `con_putc` 只走串口——printk/shell 输出通道不受影响，测试断言通道继续工作。这个"双模式输出层"是 day21-30 所有课程的结构前提。
- **`hlt` 常驻与测试收尾**：画完后 `kmain` 不再走 `0xF4` 退出，而是 `hlt` 常驻——图形终态必须留给测试器截图，`qemu-test.sh` 截屏后经 QMP 主动结束 QEMU。这是图形课与字符课测试协议的分野。

## 关键代码走读

（本课路径均相对 `days/day21-framebuffer-vbe/`）

`kernel/fb.c` 的 `fb_init` —— Dispi 设模式序列：

```c
dispi_set(DISPI_ENABLE, 0);
dispi_set(DISPI_ID, 0xB0C5);
dispi_set(DISPI_XRES, 640);
dispi_set(DISPI_YRES, 480);
dispi_set(DISPI_BPP, 32);
dispi_set(DISPI_ENABLE, DISPI_ENABLED | DISPI_LFB);
```

`kernel/fb.c` —— LFB 基址取自 multiboot_info（PCI BAR）：

```c
struct mb_fb *m = (struct mb_fb *)saved_mbi;
uint32_t base = (m->flags & (1 << 12)) ? m->fb_addr_lo : BOCHS_LFB_ADDR;
printk("[day21] framebuffer %dx%d pitch=%d at %x (mapped)\n", ..., base);
```

`kernel/fb.c` —— 页目录追加两张页表映射 LFB：

```c
uint32_t pde = base >> 22;
pgtable_fb[i]  = (base & ~0x3FFFFF) + (i << 12) | 0x3;
pgtable_fb2[i] = (base & ~0x3FFFFF) + 0x400000 + (i << 12) | 0x3;
((volatile uint32_t *)&page_dir)[pde]     = (uint32_t)pgtable_fb  | 0x3;
((volatile uint32_t *)&page_dir)[pde + 1] = (uint32_t)pgtable_fb2 | 0x3;
```

`kernel/main.c` —— 渐变与矩形（注意矩形后 `hlt` 常驻，测试器 QMP 截图后退出）：

```c
for (int y = 0; y < (int)fb_height(); y++)     /* R 随 x, G 随 y */
    for (int x = 0; x < (int)fb_width(); x++)
        fb_pixel(x, y, ((x / 3) << 16) | ((y / 3) << 8) | 0x402030);
for (int y = 200; y < 280; y++)                /* 橙色矩形 0x00FF8800 */
    for (int x = 240; x < 400; x++)
        fb_pixel(x, y, 0x00FF8800);
```

## 验收

`test/expect.txt` 三行顺序断言：

1. `framebuffer 640x480 pitch=2560 at` —— 模式设置成功、pitch 计算与 LFB 基址读取正确（基址具体值同时接受 fd000000/E0000000）；
2. `gradient painted on LFB` —— 全屏逐像素写完成（说明 LFB 页表映射生效，否则写 0xFD000000 即 #PF）；
3. `orange rect at (240,200)-(400,280)` —— 矩形绘制完成。

像素断言（`Makefile` 的 `PIXEL`，经 QEMU screendump 的 PPM 检查）：`260,220=ff8800` 验证**矩形内部**是纯橙色；`260,100=aa5540`、`320,460=ab5540` 验证**渐变公式** R=x/3、G=y/3、B=0x30（低位叠加 0x402030 后的精确色值）——即"帧缓冲内容 = 代码写的内容"。注意 `tools/pixel-assert.py` 目前缺 `main()` 调用（详见实验），像素检查当前在 `make test` 中实际为空转，断言语义以本节描述为准。

## 对照阅读

- 30dayMakeOS **day03-04**（asmhead.nas 开 GDT/切画面、`bootpack.c` 的 `init_palette`）：haribote 用汇编在启动期经 BIOS 设 320×200 模式再写 VRAM——与课程"保护模式内 Dispi 设 640×480"是同一问题的两条路线。
- 30dayMakeOS day10（`sheet` 之前的 VRAM 直写）：`boxfill8` 与本课 `fb_pixel/fb_fill` 对应。
- `source/kernel/chr_drv/console.c`：0.11 只支持文本模式（它判断 `ORIG_VIDEO_MODE`，图形卡当文本卡用）；0.11 没有 framebuffer 接口——本课起图形部分已完全超出 0.11 的能力范围，属于对 30dayMakeOS 路线的致敬。
- Multiboot 规范的 framebuffer 字段（flags bit12）见 GRUB 手册 "Boot information format"。

## 实验

1. **修复像素断言器**：`tools/pixel-assert.py` 末尾缺 `if __name__ == "__main__": main()`，当前总是空转通过。补上后重跑 `make test`，再故意把 `PIXEL` 里一个色值改错，确认能看到 `PIXEL-FAIL`——亲手把一条"假活"测试变成真测试。
2. **换分辨率**：把 `DISPI_XRES/YRES` 改成 800×600，同步改 `fb_init` 里的 `fb_w/fb_h` 与渐变边界，观察串口 pitch 变为 3200——体会"分辨率"牵动的三个常量。
