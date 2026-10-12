# day37 - 图形窗口 API:窗口操作落成系统调用

day24-30 的图形栈(fb/gfx/desktop/term)此前只服务内核自身。今天它
并入 day31 以来的进程内核, 并把窗口操作包成 **0.11 空闲调用号上的
系统调用**: `win_create=41` / `win_write=42`。从此窗口是应用的资产
—— 30dayMakeOS "API 画窗口"的思想 + 0.11 进程模型: ring3 程序经
int 0x80 申请窗口, 内核只负责合成。

## 已实现并验证(串口 21/21 + 像素断言 OK)

- 图形栈整体并入: fb.c(Bochs Dispi 设模式 + 双缓冲 + fb_getpixel)、
  gfx.c(desktop.c 图层/任务栏/拖动)、term.c(终端窗)、apps.c、
  mouse.c(PS/2 辅助通道)、splash.c、8x8 字库
- **fb_init 提前到一切任务创建之前**: LFB 的 PDE 只在引导页目录上
  挂一次, 之后的每个任务页目录(复制而来)天然可见 —— 否则 ring3
  应用触发内核写 LFB 时缺页
- LFB 基址三级兜底: PCI BAR0 实测(0xCF8/0xCFC 探 VGA 00:02.0)
  → GRUB mbi 报告值(文本模式时是 0xB8000, 不可信) → 实测值
  0xFD000000。GRUB 的 gfxpayload 对 multiboot1 不可靠, PCI 探测
  才是正道(课程版 pci.c 雏形)
- 系统调用: `win_create(x,y,w,h,name)`(ebx/ecx/edx/esi/edi 五参,
  名字经 UPTR 拷入内核) / `win_write(id,buf,len)`(gfx_putstr 写入
  窗口内容区, 写后置标记像素并 `fb_getpixel` 读回 —— 像素级证明)
- 合成调度: 应用侧改动置 `gui_dirty`, task0 主循环(合成器)发现后
  整帧重绘; `fb_flush_current()` 让窗口内容即时上屏
- 用户态 `gdemo`: ring3 程序 syscall5 建窗 → 写字 → exit;
  init 新增 `gui` 命令 fork+execve 它并 wait4 收割
- 文本控制台退场: `con_serial_only_set()` 后 con_putc = 串口镜像
  + 终端窗网格 —— 串口测试通道不受图形模式影响

启动链路: 图形模式自举 → 桌面(终端窗 + 应用窗) → init 在 ring3
读命令 → `save`(持久化) → `gui` → gdemo 建窗写字 → 标记像素读回
`ff00ff` → exit 0。像素断言核对桌面真实渲染(标题栏/应用窗/任务栏/
按钮文字四点颜色)。

## 调试实录

1. **win_write 静默失败**: gdemo 拿到的窗口"id=3", `win_rect_xy(3)`
   却查不到 —— day30 的 win_create 返回的是 **uid**(`next_uid++`,
   z 序换位后数组下标失效的修复), 而新写的 rect 查询按**下标**取。
   修复: 按 `.uid` 字段扫描。教训重演了 day30 的坑: *窗口身份只有
   uid, 任何按 wins[] 下标的查询都是错的*。
2. **断言被键盘回显劈开**: 键入窗口期, IRQ 回显字符能插进 printk
   的任意两个字符之间(串口单字节写入无锁), "ps/2 mouse ready" 被
   劈成两半。断言避开键入活跃期, 或接受劈行风险。
3. **init 启动时机**: init 在 GUI 搭建前创建, 调度后 banner 与
   kmain 的桌面打印交错, 期望序不稳 —— 移到 mouse_init 之后创建。

## 对照阅读

- `source/include/linux/`无图形 —— 0.11 是文本终端; 本章为课程
  扩展, 结构对应 30dayMakeOS day18-24(窗口层/API/计时器)
- Linux 现代对应物: fbdev(framebuffer 设备)+ 合成器; 窗口系统调用
  ≈ 简化版 Wayland surface 协议

## 课程简化

- win_write 只写一行文本(无滚动/光标); 无窗口销毁/移动 API
- 无权限: 任何应用可画任意窗口(0.11 同样无显示安全模型)
