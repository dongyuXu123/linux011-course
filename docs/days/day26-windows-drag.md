# day26 - windows-drag：窗口结构、标题栏命中与鼠标拖动

窗口"活"了。`kernel/desktop.c` 在图层系统上叠加交互：`gui_drag_step(cursor_x, cursor_y)` 在每帧处理拖动状态机——**左键按下**时从最高 z 序往下找第一个"光标落在其标题条（顶部 16px）内"的窗口并"抓住"（记录抓取偏移，并把该窗口与数组末尾交换提到最顶）；**按住移动**时窗口跟随光标（`win_move(cx-offx, cy-offy)`）；**松开**即释放。测试用 QMP 注入四步鼠标序列：移动到位 → 按下左键 → 拖动 (60,80) → 抬起，把 "window A" 从屏幕右上拖走。

串口输出：

```
[day26] first refresh done
[day26] window 'window A' grabbed
```

## 知识点

- **命中测试（hit test）**：抓取判定是 `cx ∈ [w->x, w->x+w) && cy ∈ [w->y, w->y+16)`——只认标题条。从**最高层往下**扫描（`for i = nwins-1; i >= 0; i--`）保证命中的是用户看到的那个窗口——z 序数组让"从上往下找"就是简单的倒序循环。
- **拖动偏移**：抓住瞬间记下 `drag_offx = cx - w->x`，移动中窗口位置 = 光标位置减偏移——窗口不"跳"到光标上而是保持相对抓点，这是所有 GUI 拖拽的第一体验细节。
- **提到最顶 = 数组换序**：抓住时把命中窗口与 `wins[nwins-1]` 交换——O(1) 的 z 序调整。数组序即 z 序的设计（day25）在这里兑付红利。
- **拖动状态机三态**：`drag_id < 0`（空闲，等按下）→ `drag_id >= 0`（拖动中，跟手移动）→ 松开回空闲。状态变量只有 3 个（drag_id/drag_offx/drag_offy），全部由 `mouse_buttons & 1` 驱动迁移——事件驱动 UI 状态机的最小样本。
- **button 状态的来源**：`mouse_buttons` 由 day24 的三字节包 bit0/1 更新——"按下"不是一个事件而是一个**持续状态**，直到包里该位清零。理解这点才能写出没有抖动的拖拽。
- **重绘时机**：主循环里 `gui_drag_step`（改状态）→ 坐标变了才 `gui_refresh`（呈现）。逻辑与渲染分离，拖动中每帧一次全屏重绘。
- **抓取日志即断言**：`grabbed` 一行由内核在抓取成功瞬间打印——把交互的关键状态迁移外化为串口事件，是 GUI 课"用串口验证交互"的标准手法（day24 的 `cursor at (...)`、day26 的 `grabbed` 同一思路）。

## 关键代码走读

（本课路径均相对 `days/day26-windows-drag/`）

`kernel/desktop.c` 的 `gui_drag_step` —— 抓取与命中测试：

```c
if (mouse_buttons & 1) {                       /* 左键按住 */
    if (drag_id < 0) {
        for (int i = nwins - 1; i >= 0; i--) { /* 顶窗优先 */
            struct win *w = &wins[i];
            if (w->visible &&
                cx >= w->x && cx < w->x + w->w &&
                cy >= w->y && cy < w->y + 16) {     /* 标题条命中 */
                drag_offx = cx - w->x;  drag_offy = cy - w->y;
                struct win t = wins[i];             /* 提到最顶 */
                wins[i] = wins[nwins - 1];
                wins[nwins - 1] = t;
                drag_id = nwins - 1;
                printk("[day26] window '%s' grabbed\n", ...);
                break;
            }
        }
    } else {
        win_move(drag_id, cx - drag_offx, cy - drag_offy);
    }
} else {
    drag_id = -1;                              /* 松开释放 */
}
```

`kernel/main.c` 主循环新增一行：`gui_drag_step(cursor_x, cursor_y);`（在 `mouse_task_update` 与刷新之间）。

`kernel/desktop.c` 的 `win_move` —— 拖动跟随的唯一写入口（day25 已有，本课成为热路径）：

```c
void win_move(int id, int x, int y)
{
    if (id >= 0 && id < nwins) { wins[id].x = x; wins[id].y = y; }
}
```

## 验收

`test/expect.txt` 三行顺序断言：

1. `first refresh done` —— 图层系统初绘；
2. `window 'window A' grabbed` —— **只有在"按下左键且光标位于 window A 标题条内"时才会打印**：注入序列 `80,-172; 0,0,down-left; 60,80; 0,0,up-left` 先把光标移进标题条、再按下、再拖、再抬——这行断言同时验证了命中矩形、按钮状态与抓取动作三件事；
3. 顺序在 `first refresh done` 之后：拖动发生在系统就绪之后。

像素断言（`PIXEL := 380,68=2060c0; 390,180=e8e8e8`）：拖动结束后再次采样 window A 的标题蓝与窗体灰——**窗口被拖到新位置后**颜色仍精确，证明 `win_move` + 整屏重绘没有产生撕裂或残影。（检查器现状见 day21；多步鼠标注入细节见 `tools/qemu-test.sh` 的 `--mouse` 注释。）

## 对照阅读

- 30dayMakeOS **day11**（制作窗口）：`make_window8` 画出带关闭按钮的窗口 + `make_textbox8`；其交互部分在 day26-28 的 haribote 终端窗口里逐步实现，与课程"先拖动、后终端"的顺序互为镜像。
- 30dayMakeOS day10 的 `sheet_updown`：z 序调整的系统调用版（含 sheet 池管理），对照本课的数组交换。
- 0.11 无对应物；交互事件模型的鼻祖可以看 `source/kernel/chr_drv/tty_io.c` 的等待队列——"输入驱动状态迁移"的思想同源。

## 实验

1. **只允许拖标题条外的痛点**：把命中区域改成整个窗口，体感"从窗口中间抓走"与"从标题抓走"的行为差异，再回答：为什么真实 GUI 都用标题条做拖拽热区（提示：文本选择）。
2. **拖动越界**：把 `win_move` 后加剪裁（x 不小于 0、不超屏宽），或在拖动中打印窗口实时坐标到串口，观察 60,80 注入序列下窗口的轨迹——验证"每包位移一刷新"的帧率观感。
