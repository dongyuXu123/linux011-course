# day02 - disk-loader：INT 13H 读盘，把"内核"装进内存

引导扇区只有 512 字节，装不下一个真正的内核。今天让 MBR 干它历史上真正该干的事——**加载器**：用 BIOS 的 INT 13H 磁盘服务，把紧随其后的 4 个扇区（stage2，我们的"内核雏形"）从磁盘读到内存 `0x10000`，校验读入扇区数后远跳转交权。stage2 运行起来后向串口报告自己的位置。

构建时 `Makefile` 把 `boot/bootsect.bin` 与 `boot/stage2.bin` 拼接并 `truncate` 成 1.44MB 软盘镜像（`IMG=mbr`），stage2 恰好占第 2~5 扇区。串口上会看到四行，前两行来自 MBR（实模式 `0x7C00`），后两行来自 stage2（线性 `0x10000`）：

```
[day02] boot: reading stage2 from disk (4 sectors)...
[day02] boot: read ok, jumping to 0x10000
[day02] stage2 is running at linear 0x10000
[day02] disk load verified: 4 sectors ok
```

## 知识点

- **CHS 寻址与 INT 13H AH=02**：软盘按 柱面/磁头/扇区 寻址。`CH=柱面`、`DH=磁头`、`CL=起始扇区`（1 计数）、`AL=扇区数`、`ES:BX=目标缓冲`、`DL=盘号（0x00=软盘 A:）`。返回时 CF 置位表示出错，`AL` 回报实际读到的扇区数——两者都必须检查。
- **加载地址的选择**：stage2 放在 `0x1000:0000`（线性 `0x10000`，即物理 64KB 处）。为什么不放 `0x7E00`（MBR 正后方）？0.11 的 `bootsect.s` 给出同样答案：把内核装到高地址，给引导代码和将来的 setup 留出低处的活动空间。
- **远跳转刷新 CS:IP**：`jmp STAGE2_SEG:0x0000` 同时改写 CS 与 IP。stage2 用 `[ORG 0x0000]` 汇编，因为它知道自己被装到段基址 `0x1000`、段内偏移 0 的地方——代码里的标号地址与运行地址从此一致。
- **实模式"段:偏移"与线性的换算**：线性地址 = 段寄存器 × 16 + 偏移。`0x1000:0000` = `0x10000`。这个乘 16 的规则到 day03 进保护模式后被段描述符取代。
- **镜像布局即约定**：MBR 在扇区 1，stage2 从扇区 2 开始占 4 扇区（`times 2048-($-$$)` 补齐）。扇区号是硬编码的契约，改任何一方都要同步另一方——0.11 用 `bootsect.s` 开头的一组 `.equ SYSSIZE` 宏管理同样的契约。

## 关键代码走读

（本课路径均相对 `days/day02-disk-loader/`）

`boot/bootsect.asm` —— INT 13H 读盘的完整参数摆位：

```nasm
mov ax, STAGE2_SEG
mov es, ax
mov bx, 0x0000           ; ES:BX = 1000:0000
mov ch, 0                 ; 柱面 0
mov dh, 0                 ; 磁头 0
mov cl, 2                 ; 起始扇区 2 (跳过 MBR 自己)
mov al, 4                 ; 读 4 个扇区
mov ah, 0x02
mov dl, 0x00              ; 软盘 A:
int 0x13
jc  disk_error            ; CF=1: 出错
cmp al, 4                 ; AL≠4: 少读了
jne disk_error
```

`boot/bootsect.asm` 交权：`jmp STAGE2_SEG:0x0000`。

`boot/stage2.asm` 的开头——stage2 自报家门：

```nasm
[ORG 0x0000]
stage2:
    mov ax, cs
    mov ds, ax               ; 数据段与代码段同基址
    mov si, msg_stage2
    call puts_serial
```

`Makefile` 的镜像拼接规则：

```makefile
$(BUILD)/os.img: $(BUILD)/bootsect.bin $(BUILD)/stage2.bin
	cat $^ > $@
	truncate -s 1474560 $@      # 凑满 1.44MB 软盘
```

## 验收

`test/expect.txt` 的四行顺序断言分别验证：

1. `reading stage2 from disk (4 sectors)...` —— MBR 启动并走到读盘路径；
2. `read ok, jumping to 0x10000` —— INT 13H 返回值校验通过（CF=0 且 AL=4），否则会打出 DISK READ ERROR 并 `hlt`，断言必然超时失败；
3. `stage2 is running at linear 0x10000` —— 远跳转成功、stage2 的 `[ORG]` 与加载地址吻合（地址错了会打出乱码或根本不执行）;
4. `disk load verified: 4 sectors ok` —— stage2 完整跑完。

本课无像素断言，输出全部走串口。

## 对照阅读

- `source/boot/bootsect.s` 的 `read_it` 循环——0.11 读 system 的版本：边读边检查 `ax` 是否到达 `SYSSIZE`，读到 `INT 13H` 的 64KB 边界还要处理跨道（`read_track` 子过程）。课程版把"逐道读"简化成一次 4 扇区，但"读、验、跳"的三段式完全一致。
- 0.11 把 system 装到 `0x10000` 之后又由 `setup.s` 搬到 `0x00000`（为的是让物理 0 起的实模式中断向量区让位给内核）——这是 day03 讲 A20/保护模式时的前情。

## 实验

1. **故意读坏**：把 `mov al, 4` 改成 `mov al, 5`（要读 5 个扇区，但磁盘上只有 4 个有效），观察 QEMU 是否报 `DISK READ ERROR`——理解"读到不存在的扇区"在 BIOS 层如何暴露。
2. **把 stage2 搬家**：`STAGE2_SEG` 改成 `0x2000`（线性 `0x20000`），两处 `jmp` 和 `[ORG]` 不用动（段内偏移仍为 0），跑通后画出"段值 × 16 = 线性地址"的对应表，为 day03 的平坦段（基址 0、限长 4GB）做心理准备。
