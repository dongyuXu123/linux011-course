# day03 - protected-mode：GDT、A20、CR0.PE，进入 32 位保护模式

今天离开实模式。`boot/bootsect.asm` 先用 day02 学的 INT 13H 把保护模式代码（扇区 2~3）读到 `0x7E00`，然后按 0.11 `setup.s` 后半部分的经典顺序切换：装载自己的 GDT → 打开 A20 地址线 → 置 CR0.PE → 远跳转刷新流水线。进入 32 位后直写 VGA 文本缓冲 `0xB8000` 在屏幕上留字，串口同步报告。

屏幕上（`make run`）能看到绿色（属性 0x0A）的 `day03: VGA text via 0xB8000`；串口上是三行：

```
[day03] real mode: loading PM stage, opening A20...
[day03] protected mode entered (32-bit)
[day03] VGA text buffer written, halt.
```

## 知识点

- **GDT 与平坦段**：GDT 是保护模式下"段寄存器只是下标"的查找表。本课两条描述符 `0x00CF9A000000FFFF`（代码，type 0x9A）与 `0x00CF92000000FFFF`（数据，type 0x92）都是基址 0、限长 4GB（G=4K 粒度，0xFFFFF）：段基址一律为 0 后，逻辑地址 = 线性地址，这正是"平坦内存模型"，也是 Linux 一直用到今天的段语义。
- **A20 地址线**：8086 的地址回绕兼容逻辑把第 21 根地址线（A20）默认屏蔽，不打开就只能摸到偶数 1MB。课程走最快的 Fast A20 路径：向端口 `0x92` 的 bit1 置 1。（0.11 的 `setup.s` 依次尝试键盘控制器/0x92/端口 0xEE 多种方法。）
- **CR0.PE 与流水线刷新**：`mov cr0` 置 bit0 后 CPU 处于保护模式，但流水线里可能还残留按实模式译码的指令，惯例是紧跟一条**远跳转**强制清流水线并装填 CS（`jmp dword CODE32:pm_entry`），这一跳同时完成"用新选择符加载 CS 描述符"。
- **BIOS 服务的生命线**：`int 0x10/0x13` 依赖实模式中断向量表，进入保护模式后全部失效。所以本课把所有 INT 13H 读盘动作放在切换**之前**完成；此后想再打印只能直写 `0xB8000` 或串口——这是 day04 把引导交给 GRUB 的根本原因之一。
- **CRTC 光标不影响直写**：向 `0xB8000` `stosw`（字符+属性各 1 字节）立即上屏，但硬件光标不会跟着走——管理光标是 day05 控制台的职责。

## 关键代码走读

（本课路径均相对 `days/day03-protected-mode/`）

`boot/bootsect.asm` 的切换四步（前两步在实模式，后两步跨入保护模式）：

```nasm
lgdt [gdtdesc]            ; 1. 装载 GDT
in  al, 0x92              ; 2. A20: Fast A20 门
or  al, 2
out 0x92, al
mov eax, cr0              ; 3. CR0.PE = 1
or  eax, 1
mov cr0, eax
jmp dword CODE32:pm_entry ; 4. 远跳转 → 32 位代码
```

`boot/bootsect.asm` 的 GDT——两条 8 字节描述符 + 限长：

```nasm
gdt:
    dq 0
    dq 0x00CF9A000000FFFF     ; CODE32: P|DPL0|S|type9A|G|D
    dq 0x00CF92000000FFFF     ; DATA32
gdtdesc:
    dw gdt_end - gdt - 1
    dd gdt
```

`boot/bootsect.asm` 的 `pm_entry` —— 32 位入口重装全部数据段并直写显存：

```nasm
pm_entry:
    mov ax, DATA32
    mov ds, ax                ; 6 个段寄存器全部换成平坦数据段
    ...
    mov esi, msg_vga
    mov edi, 0xB8000
    mov ah, 0x0A              ; 属性: 亮绿
.vga:
    lodsb
    stosw                     ; 字符+属性 一次 2 字节
```

注意：以上所有实模式代码（含 GDT）必须挤在**第一个扇区的 510 字节**里，魔数之后的内容（`[BITS 32]` 的 PM 代码）是第二扇区、由开头那段 INT 13H 读到 `0x7E00` 才执行的。

## 验收

`test/expect.txt` 三行顺序断言：

1. `real mode: loading PM stage, opening A20...` —— 实模式阶段（含读盘）完成；
2. `protected mode entered (32-bit)` —— GDT/A20/PE/远跳转全部成功；任何一步失败 CPU 都会跑飞或三重故障复位，串口不会出现这一行；
3. `VGA text buffer written, halt.` —— 32 位串口驱动（`puts32_serial`）工作正常，程序完整收尾。

本课无像素断言；屏幕内容（绿色 VGA 文本）供 `make run` 人工复核，测试截图同时保存在 `build/test/screen.png`。

## 对照阅读

- `source/boot/setup.s`（`end_moveX` 之后）：0.11 做同样四件事的原始现场——`cli` 后 `lgdt`/`lidt`、开 A20、置 `CR0.PE`、`jmpi 0,8` 跳给 `head.s`。我们的 `0x00CF9A...` 描述符与它 `include/asm/system.h` 里的 GDT 布局同构。
- `source/boot/head.s`：0.11 进入保护模式后的第一个 32 位文件，负责重装段寄存器、建页表——day06 的 `boot/head.S` 就是它的课程版。
- 0.11 没有走 INT 13H→`0x7E00` 这段小读盘（它由 `bootsect.s` 在更早完成）；课程把它并进 bootsect 是为了让每课只改一个文件。

## 实验

1. **拆掉 A20 会怎样**：把 `or al, 2` 注释掉再 `make test`。内核还在 1MB 内跑，多数时候"看起来正常"——试着把 `msg_vga` 写到 `0x110000` 以上的实验留到 day06 分页之后再做，先体会"为什么必须开"而不是"现在不开会立刻死"。
2. **给数据段换个 DPL**：把 `0x00CF92000000FFFF` 改成 `0x00CFFA000000FFFF`（DPL3 的用户数据段），观察加载段寄存器仍成功（ring0 可以访问 DPL3 段）——为 day13 的用户段 `0x00CFFA/0x00CFF2` 埋下伏笔。
