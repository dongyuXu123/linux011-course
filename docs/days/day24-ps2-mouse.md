# day24 - ps2-mouse：PS/2 辅助设备、三字节包与光标精灵

鼠标接管指针。`kernel/mouse.c` 走 8042 键盘控制器的辅助通道：`0x64` 写 `0xA8` 启用第二 PS/2 口、读改写控制器命令字节打开鼠标时钟位、`0xD4` 包装向鼠标发 `0xF6`（默认设置）与 `0xF4`（开始上报）。**不用中断**，主循环反复调 `mouse_task_update()` 轮询：从 `0x60` 收字节、按状态口 bit5 区分键盘/鼠标、组满 3 字节包（`[状态][dx][dy]`）后累积位移。光标用"擦-移-画"精灵：画之前先保存光标区背景（`fb_getpixel`），移动时先恢复旧背景再画新位置，避免整屏重绘。

测试通过 QMP 注入相对位移（`MOUSE := 60,40`），验证光标坐标变化。串口输出：

```
[day24] ps/2 mouse ready, move the cursor
[day24] cursor at (380,280) btn=0
```

## 知识点

- **8042 双通道架构**：端口 `0x60` 是唯一数据口，键盘与鼠标共享；靠状态口 `0x64` 的 bit5 区分（1=来自鼠标）。向鼠标发命令要经 `0x64 ← 0xD4` 的"通道选择"包装，再 `0x60 ← 命令字节`；每条命令鼠标回 ACK（0xFA），代码读出后丢弃。
- **三字节包协议**：字节 0 是状态（bit0/1=左/右键，bit4/5=x/y 符号，bit6/7=溢出），字节 1/2 是 9 位二进制补码位移的低 8 位（符号位在状态字节）。累积 `mouse_dx_acc/dy_acc` 解决"包内位移跨包"的边界问题；屏幕 y 向下所以 `dy_acc -= dy`。
- **轮询而非中断的取舍**：0.11 的软盘驱动同为轮询风格；鼠标 IRQ12 通路要处理"两设备共享 8042"的中断判别与线程化缓冲，课程内核用"主循环 + `hlt`"轮询拿到同样的功能而省掉一整套中断路径——代价是响应延迟与 CPU 占用，在 QEMU 演示场景完全够用。
- **精灵的擦-移-画**：`under[12*18]` 保存光标 12×18 区域的背景像素，移动时先按序恢复、再按 `arrow[]` 字符画（`B`=黑边、`W`=白填充、`.`=透明）画新位置。这是"局部重绘"的最小实现——day25 的图层系统会把这套思路推广到任意窗口。
- **限位剪裁**：光标坐标夹在 `[0,627]×[0,461]`（屏宽/高减去精灵尺寸），防止越界写——`fb_pixel` 内部的边界检查是第二道保险，两层防御对应"UI 语义合法"与"内存访问安全"。
- **前 5 个位移包的串口报告**：`mouse_task_update` 对前 5 个非零位移包各打一行 `cursor at (x,y) btn=n`——把不可见的坐标流变成可断言文本，是鼠标课验收的关键设计。

## 关键代码走读

（本课路径均相对 `days/day24-ps2-mouse/`）

`kernel/mouse.c` 的 `mouse_init` —— 8042 使能序列：

```c
outb_(0x64, 0xA8);                       /* 启用辅助通道 */
... 读 0x20/写 0x60: cmd |= 2;           /* 打开鼠标时钟/中断位 */
mouse_cmd(0xF6);                         /* 默认设置 */
mouse_cmd(0xF4);                         /* 开始上报 */
while (inb_(0x64) & 1) inb_(0x60);       /* 排空残余 */
```

`kernel/mouse.c` 的 `mouse_task_update` —— 三字节组包：

```c
uint8_t st = inb_(0x64);
if (!(st & 1)) return;           /* 无数据 */
uint8_t d = inb_(0x60);
if (!(st & 0x20)) return;        /* 键盘数据, 不归我们 */
pkt[mouse_cycle++] = d;
if (mouse_cycle < 3) return;
mouse_cycle = 0;
if (pkt[0] & 0xC0) return;       /* 溢出丢弃 */
```

`kernel/mouse.c` —— 精灵的保存/恢复（擦-移-画）：

```c
if (drawn) { /* 用 under[] 恢复旧区域 */ }
cursor_x += mouse_dx_acc;  cursor_y += mouse_dy_acc;
for (...) under[j*12+i] = fb_getpixel(cursor_x + i, cursor_y + j);
for (...) if (arrow[j][i]=='B') fb_pixel(..., 0);
         else if (arrow[j][i]=='W') fb_pixel(..., 0x00FFFFFF);
```

`kernel/main.c` 主循环：`mouse_task_update(); hlt;`——鼠标维护成为主循环的第一个常驻任务，为 day25 的刷新调度与 day26 的拖动状态机占好位置。

## 验收

`test/expect.txt` 三行顺序断言：

1. `ps/2 mouse ready, move the cursor` —— 8042 使能与上报开启完成（鼠标若没 ACK 成功，后续不会有位移包）；
2. `cursor at (380,280) btn=0` —— 注入 `MOUSE := 60,40`（QEMU 相对位移）后，光标从初始 (320,240) 移到 **(380,280)**——坐标精确匹配证明三字节包解析、符号扩展、y 轴翻转全部正确（`mouse.c` 对前 5 个位移包各打一行，expect 取其中代表）；
3. 无像素断言本课未启用（光标形状人工看截图），`build/test/screen.png` 可见白色箭头精灵位于背景上。

## 对照阅读

- 30dayMakeOS **day07**（鼠标）：haribote 的 `enable_mouse`/`mouse_decode` 与本课 `mouse_init`/组包逻辑同源——连"等到 3 字节凑齐"的状态机都一样；它走 IRQ12 中断，课程用轮询。
- 30dayMakeOS day10 的 `sheet` 光标搬移（`sheet_slide`）是本课"擦-移-画"的系统化版本——day25 对照。
- `source/kernel/chr_drv/keyboard.S`：0.11 同样要处理"0x60 口上谁是数据主人"的问题（它只管键盘；PS/2 鼠标在 0.11 时代尚未普及，没有驱动）——本课的 bit5 判别补上了这一历史空白。
- `source/kernel/blk_drv/floppy.c`：0.11 里另一处"轮询等设备"的样本（软盘马达与中断等待），轮询风格可互为印证。

## 实验

1. **打开鼠标中断**：把控制器命令字节的 bit1 已置位的基础上，给 IRQ12 注册处理器（`register_irq(12, ...)`，记得放开 `0xA1` 的掩码位），把轮询改成中断驱动——对比两种实现的延迟与代码复杂度。
2. **中键与滚轮**：状态字节 bit2 是中键；把 `mouse_btn` 打印改成三键位掩码，注入带按键的位移（`qemu-test.sh` 的鼠标事件支持 `down-left/up-left`），验证 bit0/1 的键位映射。
3. **加速度曲线**：把 `mouse_dx_acc` 乘个系数（如 2）模拟"指针加速"，再跑测试观察 `cursor at` 断言如何失败——验收坐标与输入处理逻辑的耦合关系一目了然。
4. **包计数器**：在 `packets++` 后加一个每 100 包打印一次的统计行，量化 QEMU 下鼠标事件的真实频率——为后续帧率优化提供手感依据。
