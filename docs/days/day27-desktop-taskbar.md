# day27 - desktop-taskbar：桌面壳、任务栏与 jiffies 时钟

给窗口群一个"家"。`kernel/desktop.c` 的 `gui_refresh` 在图层栈里插入任务栏层：屏幕底部 24px 的深蓝条（`0x00181830`），左侧为每个窗口渲染一个任务按钮（按钮宽 = 标题字数×8+12px，灰底亮字），右侧用 day08 的 jiffies 折算成 `mm:ss` 时钟并按 8px 网格右对齐。窗口仍可拖动（day26 逻辑保留），任务栏在窗口之下、光标之上——刷新顺序变为 背景 → 窗口 → 任务栏 → 光标。

测试注入含一次点击的长序列（`MOUSE := 80,-172; 0,0,down-left; 60,80; 0,0,up-left; 0,0,click-left`）：拖动 window A 后再点一下。串口输出：

```
[day27] first refresh done
[day27] window 'window A' grabbed
```

任务栏文案与时钟都是 day22 的 `gfx_putstr` 逐字画上去的——字符网格（8px）是本课所有排版的度量衡。

## 知识点

- **桌面进程的雏形**：虽然代码仍跑在 `kmain` 主循环里，但 `gui_refresh` 已经是"桌面壳"的渲染函数——墙纸（渐变+棋盘格）、窗口层、任务栏层、时钟，一个桌面环境的四件套齐了。真实桌面（如 Xfce）里这些分属 WM 与 panel 进程，课程刻意单进程化以聚焦渲染本身。
- **任务栏的布局算法**：按钮从 x=4 起排，每个宽 `strlen(title)*8+12`（8px 字格 + 6px 左右留白），间隔 4px——纯代码布局，无配置文件。右侧时钟锚定 `w - 5*8 - 8`（5 字符 `mm:ss` 的像素宽 + 8px 边距）。
- **时钟取自 jiffies**：`sec = jiffies / 100`（PIT 固定 100Hz），再拆 `mm:ss`。0.11 的时区是 `kernel/mktime.c` 从 CMOS 起算开机秒数——课程用 jiffies 免去 RTC 读数，但"系统时间的唯一来源是节拍中断"这一层语义相同。
- **任务按钮与 z 序的错位**：按钮按 `wins[]` 数组顺序渲染，数组末尾是"最顶层窗口"但按钮却排在最右——真实任务栏会按"激活顺序"排按钮并高亮当前窗口。这是刻意留下的不完美，实验题见文末。
- **刷新序的最终形态**：`背景 → wins[] → draw_taskbar() → draw_cursor()`。任务栏画在窗口**之后**，所以窗口拖到屏幕底部会被任务栏盖住——层的顺序就是遮挡关系，一条 for 循环即物理规律。
- **24px 高度的构成**：按钮 18px + 上下各 3px 边距恰好填满栏高；8px 字格的字在 `h-th+8` 处起排，垂直方向大致居中。栏高、字号、按钮高三个常量互相咬合，改任何一个都要重排。
- **点击区域的预留**：`MOUSE` 注入序列末尾的 `click-left` 当前不触发任何逻辑——任务栏尚无命中处理（day26 的 `gui_drag_step` 只认标题条）。这为实验题"点击任务按钮切窗口"留好了挂点。

## 关键代码走读

（本课路径均相对 `days/day27-desktop-taskbar/`）

`kernel/desktop.c` 的 `draw_taskbar` —— 任务栏与按钮：

```c
int th = 24, w = fb_width(), h = fb_height();
fb_rect(0, h - th, w, th, 0x00181830);      /* 栏底 */
fb_hline(0, h - th, w, 0x00404070);         /* 顶边高光 */
int bx = 4;
for (int i = 0; i < nwins; i++) {
    int bw = strlen(t) * 8 + 12;            /* 8px 字格 + 留白 */
    fb_rect(bx, h - th + 3, bw, 18, 0x00303058);
    gfx_putstr(t, bx + 6, h - th + 8, 0x00E0E0FF, 0x00303058);
    bx += bw + 4;
}
```

`kernel/desktop.c` —— 右侧时钟（jiffies → mm:ss）：

```c
uint32_t sec = jiffies / 100;
char buf[10];
int m = sec / 60, s2 = sec % 60;
buf[0]='0'+m/10; buf[1]='0'+m%10; buf[2]=':';
buf[3]='0'+s2/10; buf[4]='0'+s2%10; buf[5]=0;
gfx_putstr(buf, w - 8*5 - 8, h - th + 8, 0x00FFFFFF, 0x00181830);
```

`gui_refresh` 的最终层序：背景 → `for wins[]` → `draw_taskbar()` → `draw_cursor()`。

`kernel/main.c` 的注入剧本（`MOUSE` 序列五段）：`80,-172` 移到 window A 标题条 → `down-left` 抓住 → `60,80` 拖动 → `up-left` 释放 → `click-left` 点击（暂无逻辑）——同一序列复验 day26 的拖动与本课的层序稳定。

## 验收

`test/expect.txt` 三行顺序断言：`first refresh done`（含任务栏的首刷）与 `window 'window A' grabbed`（拖动回归）。注入序列末尾追加的 `click-left` 确认点击不破坏拖动状态机（松开后 drag_id 归 -1）。

像素断言（`PIXEL := 60,468=181830; 380,68=2060c0`）：

- `60,468=181830` —— **任务栏本体色**（屏高 480 减 24px 栏高，y=468 落在栏内）——任务栏存在性与配色验证；
- `380,68=2060c0` —— window A 标题条（拖动回归 + 窗口层在任务栏之下的层序抽查）。

（检查器现状见 day21；截图可人工核对按钮文本与时钟 `mm:ss`。）采样点 `y=468` 特意取在时钟文字下方 4px 的纯色区——若时钟排版出错压到采样行，断言也会红。

## 对照阅读

- 30dayMakeOS **day14-18**：haribote 的"任务栏"演化——day14 画窗口计数/内存显示的底栏、day17 命令行窗口、day18 `fwrite`/窗口色；课程把"面板+时钟"压缩进一课，对应它的 day14（底栏信息）+ day18（系统信息窗口）。
- 30dayMakeOS day16（窗口切换/计数）：任务按钮"点击切换窗口"的对应物，本课留给实验。
- `source/kernel/mktime.c` + `source/init/main.c` 的 `START_TIME`：0.11 用 CMOS 时间戳做 uptime 基准——与本课 `jiffies/100` 对照，理解"墙上时间 vs 单调时间"。
- 0.11 没有任务栏与桌面概念（文本控制台即全部界面）；本课起的桌面壳属于课程自研层，与其余 GUI 课一样以 30dayMakeOS 为参照系。

## 实验

1. **点击任务按钮切窗口**：在 `gui_drag_step` 里加"按下位置落在任务栏按钮矩形内且不在拖窗状态"的分支，把对应窗口 `win_top` 提到最顶——补齐任务栏的另一半职责。
2. **高亮激活窗口**：给 `struct win` 加 `active` 字段，被抓住/提到最顶的窗口按钮画亮色底——顺手处理"拖动时哪个按钮该亮"的状态同步问题。
3. **栏高实验**：把 `th` 从 24 改成 32，按钮与时钟的 y 坐标都要联动（现在是 `h - th + 常量` 的硬编码）——把这两个偏移改成基于 `th` 的表达式，体会"布局参数化"的必要性。
4. **时钟闪烁冒号**：让 `mm:ss` 的冒号每秒闪一次（按 `jiffies/100 % 2` 选颜色重画时钟区）——最小代价验证"局部重绘"在你手里是否可行。
