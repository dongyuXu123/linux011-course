# day30 - release：启动画面、进度条与发布版桌面

30 天收官。新增 `kernel/splash.c`：`fb_init` 之后、桌面建立之前播放启动画面——深色底（0x00101418）上标题条写 `linux011-course` 与 `30 days from MBR to desktop`，下方**30 格进度条逐格点亮**（每格 12px、靠 `jiffies` 每 6 tick 一格，约 1.8 秒），落款 `v0.30 (c) the course, GRUB booted`；停 1 秒后进入桌面。桌面本身是 day28/29 的完全体：terminal + memory monitor + 两个可拖窗口 + 任务栏时钟，`ver` 升级为 `linux011-course 0.30 gui`。

测试注入 `⏎/help⏎/ver⏎`，等待 splash 后断言桌面状态。串口输出：

```
[day30] splash shown, entering desktop
[day30] first refresh done
builtins: help ps ver
linux011-course 0.30 gui
```

## 知识点

- **splash 的时间驱动**：进度条每格 `while (jiffies - t0 < 6) hlt;`——用节拍中断当秒表而不是忙等循环计数，CPU 保持在睡眠态。`jiffies` 的无符号回绕安全（`t0 + 6` 的差值比较）是内核计时的标准写法，0.11 的 `timer_interrupt` 语义相同。
- **版本即验收**：`0.30 gui` 同时出现在 splash、终端 `ver` 与 expect 断言里——发布物的身份标识由代码、显示、测试三方共同背书。30 天的版本链 `0.20 (terminal milestone) → 0.28 gui → 0.30 gui` 直接映射里程碑。
- **发布镜像 = 每课快照的固化**：`make -C days/day30-release` 产出 `build/os.iso`（GRUB + kernel.flat + ATA initrd），`make test` 跑全链路验收——"每天一个可运行产物"的承诺兑现到最后一天。全仓 `make test` 可输出 30 课测试矩阵。
- **启动时序的完整闭环**：`GRUB(aout-kludge) → head.S 分页 → trap/irq/timer/syscall/tty/ramfs/ata → fb_init(Dispi+LFB 映射) → splash → 桌面窗口 → ring3`（用户态 init 链保留在代码中）。day01 的串口驱动到 day30 的 splash 用的是同一套 `outb`——从 MBR 到桌面没有任何一步被替换掉，只有增层。
- **动画的最小实现**：进度条没有双缓冲也没有定时器回调——"画一格 + 睡 6 tick"的同步节奏在单线程内核里完全够用。真正的合成器要把动画帧与重绘解耦，那是超出课程范围的下一步。
- **`hlt` 期间的串口断言**：splash 播放约 2.8 秒里 CPU 大部分时间在 `hlt` 睡眠，但 PIT 中断照常唤醒更新 jiffies——测试器的串口日志与 QMP 截图都不受影响，动画不阻塞观测。

## 关键代码走读

（本课路径均相对 `days/day30-release/`）

`kernel/splash.c` 的 `splash_show` —— 标题与 30 格进度：

```c
fb_fill(0x00101418);
fb_rect(140, 190, 360, 60, 0x00182030);
gfx_putstr("linux011-course", 226, 200, 0x00FFFFFF, 0x00182030);
gfx_putstr("30 days from MBR to desktop", 190, 216, 0x0088CCFF, ...);
for (int d = 0; d < 30; d++) {                 /* 30 格 = 30 天 */
    fb_rect(146 + d * 12, 268, 8, 12, 0x0030A040);
    uint32_t t0 = jiffies;
    while (jiffies - t0 < 6)                   /* 每格 6 tick ≈ 60ms */
        __asm__ volatile("hlt");
}
```

`kernel/main.c` —— splash 插在 fb_init 与桌面之间：

```c
fb_init();
con_serial_only_set();
splash_show();
printk("[day30] splash shown, entering desktop\n");
win_create(380, 60, 220, 130, 0x002060C0, "window A");
... term_attach / apps_init / gui_refresh ...
for (;;) { mouse_task_update(); term_shell_poll(); gui_drag_step(...); ... }
```

`kernel/term.c`：`ver` 分支输出 `linux011-course 0.30 gui`。

`kernel/desktop.c` 与 `kernel/apps.c`：day27/28/29 的层序与应用模型原样沿用——发布课不重写任何下层代码，只做总装与润色，这是"增量快照"承诺的最后一次兑现。

## 验收

`test/expect.txt` 四行顺序断言（`KEYS := ret,h,e,l,p,ret,v,e,r,ret`）：

1. `splash shown, entering desktop` —— splash 完整播放（30 格 × 6 tick ≈ 1.8s + 停 1s）后进入桌面——**测试必须等过这段真实时间**，timeout 设为 90s 留足余量；
2. `first refresh done` —— 完全体桌面（3 窗口 + 任务栏 + 应用）初绘成功；
3. `builtins: help ps ver` / `linux011-course 0.30 gui` —— 终端链路与发布版本号。

像素断言（`PIXEL := 60,468=181830; 370,280=602080`）：任务栏底色 + memory monitor 标题紫——splash 退场后桌面层序与配色无损（splash 没有残留在桌面上）。检查器现状说明见 day21；`build/test/screen.png` 可核对 splash 截图（`screen2.png` 为桌面终态）与进度条形态。

`result.json` 的发布语义：`{"name":"day30-release","status":"PASS",...}` 是 30 课矩阵的最后一格——`qemu_exit`、串口断言、截图三件齐全，发布物才算成立。

## 对照阅读

- 30dayMakeOS **day29-30**（润色）：haribote 的 day29（保护倒计时/性能计）与 day30（`color2/haribote 商业版`收尾）——"最后一天做启动画面与打磨"的节奏与课程一致；haribote 的进度条画在应用窗口里，课程的画在 splash 上。
- day01-29 各课讲义：本课是全部机制的总装，任何一环的对照阅读都在当天讲义里。
- `source/init/main.c` 末尾：0.11 启动打印 `Loading ... done.` 后进入 shell 循环——课程的 splash 是它的图形时代回声；0.11 的版本身份在 `include/linux/version.h` 的 `UTS_RELEASE "0.11"`，与 `ver` 的 `0.30 gui` 对位。

## 实验

1. **进度条加速 10 倍**：把 `while (jiffies - t0 < 6)` 的 6 改成 1（或加环境变量式宏），重跑测试并核对 expect 首行出现得更早——理解 splash 时长如何进入测试预算（TIMEOUT 的取值依据）。
2. **在 splash 上显示启动耗时**：在 `kmain` 开头记 `jiffies`，splash 里把 `自上电到 splash 的 tick 数` 打进落款行——顺手验证 jiffies 从 `head.S` 之后一直在走（day07 的成果贯穿 30 天）。
3. **全仓回归**：在仓库根目录跑 `make test`，确认 30 课测试矩阵全绿——这是毕业验收：任何一课红了你都知道该回哪天的讲义。
