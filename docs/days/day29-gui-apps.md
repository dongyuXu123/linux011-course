# day29 - gui-apps：小应用窗口——内存监视与实时数据

桌面上第一代"应用"。新增 `kernel/apps.c`：`apps_init()` 注册一个 "memory monitor" 窗口（紫色标题，360,250 处 240×110）；`apps_render()` 在每次 `gui_refresh` 的窗口层之后被回调，向内容区画三行**实时数据**——`free pages: N`（每帧读 day10 的 `mem_free_pages()`）、`uptime: N s`（jiffies/100）、`cursor: N`（光标 x 坐标）。term 终端（day28）与 `help/ps/ver` 全部保留，测试注入 `help⏎`、`ver⏎` 复验。

"应用"的定义在这里很朴素：**一个窗口 + 一个每帧刷新的 render 回调**。串口输出：

```
[day29] first refresh done
builtins: help ps ver
linux011-course 0.28 gui
```

## 知识点

- **render 回调模型**：`apps_render` 由 `gui_refresh` 在画完窗口框后调用——应用不直接碰屏幕时序，只在桌面的重绘节拍里"领取"自己的绘制机会。这与 30dayMakeOS 各 demo 窗口在 `task_b_main` 里定时重绘、以及现代 GUI 的 invalidate/expose 事件是同一模型。
- **数据源即内核状态**：`mem_free_pages()`（day10 位图统计）、`jiffies`（day07 心跳）、`cursor_x`（day24 光标）——应用窗口本质是内核状态的**可视化视图**。因为单地址空间，"读内核变量"就是应用的数据访问层；0.11/现代系统里这要绕 /proc、系统调用或共享内存。
- **数字转字符串的手工循环**：`apps_render` 里三段 `while (v) { n[i++] = '0' + v%10; v/=10; }` + 倒序——没有 vsprintf 的 `%d` 可用（apps.c 不含 printk 缓冲），演示了"格式化"在最小依赖下的写法；也解释了为什么先 `if (v==0) n[i++]='0'`。
- **窗口坐标 → 内容区坐标**：`term_window_rect_for(mem_win_id, &x,&y,&w,&h)` 拿到窗口矩形后，文本从 `(x+8, y+24)` 起排（避开标题条 16px + 8px 边距）——"内容区坐标系"约定由 desktop 层统一维护，应用只管相对偏移。
- **刷新链的完整时序**：`gui_refresh` = 背景 → wins[]（框）→ `term_render()`（终端内容，受 dirty 保护）→ `apps_render()`（应用内容，每帧重画）→ 任务栏 → 光标。同一帧里"静态内容带脏标记、动态内容每帧重算"，按数据变化频率分层是渲染优化的第一课。
- **free pages 数字会变**：day10 自检后页位图几乎全空，应用窗口显示的是真实位图统计（几千页量级）——这个数字随任何分配/释放实时变化，是"应用在读活数据"的直接证据。

## 关键代码走读

## 关键代码走读

（本课路径均相对 `days/day29-gui-apps/`）

`kernel/apps.c` 的注册与渲染：

```c
static int mem_win_id = -1;
void apps_init(void)
{
    mem_win_id = win_create(360, 250, 240, 110, 0x00602080, "memory monitor");
}
void apps_render(void)
{
    if (!term_window_rect_for(mem_win_id, &x, &y, &w, &h)) return;
    /* free pages / uptime / cursor 三行实时数据 */
    gfx_putstr(buf, x + 8, y + 24, 0x00202020, 0x00E8E8E8);
}
```

`kernel/apps.c` —— 手工 itoa（无 vsprintf 依赖）：

```c
char n[12]; int i = 0, v = (int)mem_free_pages();
if (v == 0) n[i++] = '0';
while (v) { n[i++] = '0' + v % 10; v /= 10; }
{ int a=0, b=i-1; while (a<b) { swap; } }      /* 倒序 */
n[i] = 0;  strcat(buf, n);
```

`kernel/desktop.c` 的刷新链（窗口层之后的两个回调）：

```c
term_render();        /* 终端内容: dirty 才画 */
apps_render();        /* 应用内容: 每帧重画 */
draw_taskbar();
```

## 验收

`test/expect.txt` 四行顺序断言（`KEYS := h,e,l,p,ret,v,e,r,ret`）：`first refresh done` → `builtins: help ps ver` → `linux011-course 0.28 gui`——终端链路回归 + 应用窗口并存（`ver` 沿用 0.28 标识）。

像素断言（`PIXEL := 60,468=181830; 370,280=602080`）：

- `60,468=181830` —— 任务栏回归；
- `370,280=602080` —— **memory monitor 窗口的紫色标题条**（0x00602080）：采样点落在标题字库笔画之外，验证窗口框本体——应用窗口的存在性由像素背书，内容数字的动态部分由串口镜像不可行（apps_render 不走串口），以截图人工复核。

（检查器现状说明见 day21。）

## 对照阅读

- 30dayMakeOS **day22-25**（应用）：haribote 的 `task_b_main` 周期重绘窗口内容（day22 色块/日历、day23-24 计算器与文本查看器）——"窗口 + 定时刷新内容"的应用模型同源；课程用单一 desktop 循环替代它的多任务渲染。
- 30dayMakeOS day25（Ctrl/Shift 与文本编辑器）应用的输入侧：课程的应用是纯只读视图，输入侧仍集中在终端——对比"应用自有焦点"的下一阶段。
- `source/fs/proc` 不存在于 0.11；0.11 用户程序看系统状态靠 `ps` 类工具读内核表（无此工具）或 `/dev/kmem`——课程"直接读内核变量"是单内核快照的特权，写讲义时应点明其不可移植性。

## 实验

1. **加一个"taskbar 时钟"应用窗口**：用 `apps.c` 的模型再加一个窗口显示 `hh:mm:ss`（jiffies 换算到 24 小时制，起步时刻自定）——练一遍"注册窗口 + render 回调 + 内容区排版"三件套。
2. **让 mem 窗口可拖动且数据不断更**：用 QMP 注入拖动序列后观察截图里 free pages 数字是否仍在更新——验证 `term_window_rect_for` 在窗口移动后返回新坐标（render 与位置解耦的证明）。
