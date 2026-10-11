# day01 - hello-mbr：512 字节引导扇区

今天从零开始写"最小可运行产物"：一个恰好 512 字节的引导扇区。BIOS 加电自检后，会把磁盘第一个扇区搬到内存 `0x7C00` 并跳过去执行——前提是扇区末尾两字节是魔数 `0xAA55`。我们的"内核"就是这 512 字节本身，它做三件事：初始化串口 COM1（115200 8N1）、向串口打印问候语、再用 BIOS INT 10H 在屏幕上打印同样内容。

构建产物是软盘镜像（`IMG=mbr`），`make run` 打开 QEMU 窗口能亲眼看到屏幕上的字；`make test` 则无头启动 QEMU，从 COM1 串口日志里做顺序断言——这条串口通道将贯穿全部 30 课，是所有自动验收的依据。串口上会看到：

```
[day01] hello from MBR (COM1 115200)
day01: MBR is running on screen too        ← 这行走的是 BIOS 打字
[day01] boot sector done, halt.
```

## 知识点

- **BIOS 引导约定**：BIOS 只负责把 512 字节的第一个扇区加载到 `0x7C00:0000`（实模式 16 位），检查第 510~511 字节的 `0xAA55` 魔数，然后 `jmp 0x7C00`。引导扇区必须以 `times 510-($-$$) db 0` 补齐再加 `dw 0xAA55`，少一个字节 BIOS 就拒绝引导。
- **16550 串口编程**：COM1 基址 `0x3F8`。设置波特率要先置线路控制寄存器（`COM1+3`）的 DLAB 位（`0x80`），再向 `COM1+0/1` 写 16 位除数——除数 = 1 时波特率 115200。随后 `COM1+3` 写 `0x03`（8 数据位、无校验、1 停止位），`COM1+2` 开 FIFO，`COM1+4` 置 DTR|RTS|OUT2。
- **轮询式发送**：向 `COM1`（数据口）写一个字符前，要先读线路状态寄存器 `COM1+5`，等 bit5（THR 空闲）置位。没有中断、没有缓冲管理的"忙等"是最早也最可靠的输出方式。
- **实模式段寄存器初始化**：进入时只有 CS 是可信的，DS/ES/SS 必须自己清零；栈指针设在 `0x7C00` 往下长——栈正好顶在自己代码下面，不会踩到 `0x7C00` 之前的 BIOS 数据区以外的地方。
- **串口与屏幕双通道**：BIOS INT 10H（AH=0x0E teletype）输出给人眼，串口输出给测试器。两路并行是课程"自动验收 + 人工观察"双重目标的起点。
- **`$` 与 `$$` 的NASM语义**：`$` 是当前节内偏移、`$$` 是节起始——`times 510-($-$$) db 0` 正是"从开头数到第 510 字节"的填充术。配合 `[ORG 0x7C00]`，所有标号地址都与 BIOS 的加载地址吻合。
- **为什么先 `cli` 再设段寄存器**：加电后 BIOS 可能留有未决的硬件中断，`ss:sp` 尚未就绪时被中断会写坏内存。本课开头 `cli → 设段寄存器 → mov sp → sti` 的顺序是实模式初始化的标准防弹序列。

## 关键代码走读

（本课路径均相对 `days/day01-hello-mbr/`）

`boot/bootsect.asm` 的 `serial_init` —— 115200 8N1 的完整初始化序列：

```nasm
mov dx, COM1 + 3
mov al, 0x80              ; DLAB=1, 允许设置波特率
out dx, al
mov dx, COM1
mov al, 0x01              ; 除数低字节 = 1 → 115200
out dx, al
```

`boot/bootsect.asm` 的 `putc_serial` —— 轮询发送，等 LSR bit5：

```nasm
.wait:
    in al, dx                ; COM1+5: 等发送保持寄存器空
    test al, 0x20
    jz .wait
```

`boot/bootsect.asm` 的 `puts_bios` —— BIOS teletype，QEMU 窗口可见：

```nasm
mov ah, 0x0E
mov bx, 0x0007            ; 页0, 黑底白字属性
int 0x10
```

`boot/bootsect.asm` 的结尾 —— 510 字节填充与魔数：

```nasm
times 510 - ($ - $$) db 0   ; 填充到 510 字节
dw 0xAA55                    ; 引导魔数
```

主流程只有三步：`call serial_init` → `puts_serial`/`puts_bios` 各打一轮 → `hlt` 死循环挂起（`.halt: hlt; jmp .halt`，hlt 后仍可能被外部中断唤醒，循环保证不再往下执行）。

## 验收

`test/expect.txt` 对 QEMU 串口日志做**顺序断言**（`tools/expect-match.py`：每行必须按顺序出现，允许行内只匹配前缀）：

1. `[day01] hello from MBR (COM1 115200)` —— 串口初始化与轮询发送正确；
2. `[day01] boot sector done, halt.` —— 程序完整跑完没有在中途挂死。

本课没有像素断言（VGA 屏幕内容走 BIOS 打字，不便于机器判读，所以真正可断言的通道选了串口）。

## 对照阅读

- `source/boot/bootsect.s` —— Linux 0.11 的引导扇区做的是同一件事的"加强版"：把自己从 `0x7C00` 搬到 `0x90000`，再用 INT 13H 把 setup 和 system 读进内存。day01 的串口驱动在 0.11 里对应 `source/kernel/chr_drv/serial.c` 的 `rs_init()`/`rs_write()`（0.11 用中断驱动，课程用轮询）。
- BIOS INT 10H 在 0.11 进入保护模式后就不可用了（那是实模式 BIOS 服务），所以 0.11 的输出走 `0xB8000` 显存和串口——正是 day03/day05 的内容。

## 实验

1. **把波特率改成 9600**：改 `boot/bootsect.asm` 里 `serial_init` 的除数——`mov al, 0x01` 换成 `mov al, 0x0C`（115200/9600 = 12），高字节仍为 0。QEMU 的串口后端自适应波特率，输出内容不变，但你可以借此理解 DLAB 两步写时序。
2. **给 BIOS 输出换颜色**：`puts_bios` 里 `mov bx, 0x0007` 的高字节是属性（低 4 位前景色、高 4 位背景色），改成 `0x1F` 试试蓝底白字；再体会为什么串口通道没有"属性"这种东西。
