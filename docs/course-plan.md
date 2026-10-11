# Linux 0.11 课程重构方案：30 天渐进式动手轨

> 设计蓝本：[yourtion/30dayMakeOS](https://github.com/yourtion/30dayMakeOS)（《30 天自制操作系统》）
> 结合对象：本仓库的 Linux 0.11 源码精读轨（`source/` + `docs/`）
> 硬性要求：**每一课在 QEMU 中自动测试通过**；**GRUB 引导**；**渐进增量、每课可独立复现**；**最终包含图形桌面**。

## 一、借鉴 30dayMakeOS 的三条教学思想

1. **每天一个可运行产物**：不做"读完才懂"的课程。每课结束都有一个能开机、能观察、能被自动验收的系统；课程进度 = 系统的生长史。
2. **增量快照，永不回头修补**：每课目录是当日完整源码快照（`days/dayNN-*/`），互相独立可构建。学第 N 课不需要第 N-1 课的上下文状态，`make -C days/dayNN test` 即可复现。
3. **先见到效果，再解释原理**：每课的讲义先给"今天屏幕上发生了什么"，再回到 Linux 0.11 原始源码（`source/`）对照精读，最后布置实验。

## 二、双轨结构

| 轨道 | 位置 | 说明 |
|---|---|---|
| **动手轨（新增主线）** | `days/day01 … day30` | 30 天从 MBR 到图形桌面，现代工具链（gcc -m32 / GAS / NASM / GRUB 2 / QEMU） |
| **精读轨（保留）** | `source/` + `docs/` | karottc/linux-0.11 原始源码（赵炯注释）+ 既有模块文档、启动串讲、调用图 |

两轨通过每课讲义（`docs/days/dayNN.md`）的"对照阅读"栏目互链：动手轨写到某个子系统时，指向 0.11 的对应文件（如 day11 讲调度 → `source/kernel/sched.c`）。

## 三、30 天日程

### 第一周　引导与硬件（day01–07）——汇编期，GRUB 在 day04 登场
| 天 | 课题 | 内容 | 对照 0.11 |
|---|---|---|---|
| 01 | hello-mbr | 512 字节引导扇区：UART 初始化、直接写 COM1 与 BIOS 打字 | `boot/bootsect.s` 的雏形 |
| 02 | disk-loader | INT 13H 读盘：把内核扇区装进内存再跳转 | bootsect.s 的 `read_it` 循环 |
| 03 | protected-mode | GDT、A20、CR0.PE：进入 32 位并直写 VGA 文本缓冲 | `boot/setup.s` 后半 |
| 04 | **grub-multiboot** | Multiboot 规范头(aout-kludge 平坦镜像)、grub.cfg、grub-mkrescue 生成可启动 ISO——**从今天起 GRUB 取代自写引导** | bootsect+setup 的"现代替代品" |
| 05 | c-runtime | C 运行环境：栈、BSS 清零、early printk、解析 Multiboot memory map | setup.s 收集的内存参数 |
| 06 | paging-idt | 0.11 式 head：4 张页表恒等映射 16MB、IDT、除零异常演示 | `boot/head.s` |
| 07 | timer-pit | 8253/8259：可编程定时器、jiffies 心跳、中断现场 | `kernel/sched.c` 的 timer_interrupt |

### 第二周　内核核心（day08–14）——进入 C 内核，按 0.11 模块生长
| 天 | 课题 | 内容 | 对照 0.11 |
|---|---|---|---|
| 08 | keyboard-tty | 键盘中断 → 环形缓冲 → tty 层 → 控制台回显 | `kernel/chr_drv/` |
| 09 | printk-vsprintf | printk + vsprintf 移植（%d %x %s %c） | `kernel/printk.c` `vsprintf.c` |
| 10 | memory-manager | mem_map 物理页管理、页缺失处理、恒等映射下的按需分配 | `mm/memory.c` |
| 11 | tasks-scheduler | task_struct、软件上下文切换、时钟轮转调度（0.11 的 ljmp-TSS 切换在讲义中对照） | `kernel/sched.c` |
| 12 | fork-exit | fork/exit/wait：任务复制与回收 | `kernel/fork.c` `exit.c` |
| 13 | syscall-ring3 | int 0x80 系统调用门、特权级 3、a.out 式用户程序 | `kernel/system_call.s` |
| 14 | kernel-shell | 行编辑（退格/回车）+ 内置命令 shell（help/ps/uptime/clear） | 0.11 的 `/bin/sh` 前身 |

### 第三周　文件系统与用户态（day15–20）
| 天 | 课题 | 内容 | 对照 0.11 |
|---|---|---|---|
| 15 | ramfs | 超级块/inode/bitmap 模型的内存文件系统，mount 框架 | `fs/super.c` `bitmap.c` |
| 16 | ramfs-files | 文件读写、shell 增加 ls/cat/echo/write | `fs/file_dev.c` `read_write.c` |
| 17 | exec-aout | 从 ramfs 加载用户程序执行（a.out 平坦格式） | `fs/exec.c` |
| 18 | grub-initrd | initrd 挂第二块 IDE 盘，内核用 ATA PIO 轮询读入 ramfs（GRUB2 的 multiboot1 `module` 实测不可用，讲义记录了这个坑） | 0.11 的软盘/硬盘根文件系统与 hd.c |
| 19 | user-init | 用户态 /sbin/init：fork-exec-sh、rc 脚本、登录横幅 | `init/main.c` 的 init() |
| 20 | milestone-terminal | 字符终端里程碑：中断/调度/内存压测自检，串口输出系统报告 | — |

### 第四周　图形桌面（day21–30）——呼应 30dayMakeOS 的收官之路
| 天 | 课题 | 内容 | 对照 30dayMakeOS |
|---|---|---|---|
| 21 | framebuffer-vbe | QEMU/Bochs VBE Dispi 端口设 640×480×32、LFB(PCI BAR 0xFD000000) 内核页表扩展映射 | day03–04（画面显示） |
| 22 | font-rendering | 8×16 ASCII 点阵字体、putfont/putstr 到帧缓冲 | day07–08（字符显示） |
| 23 | drawing-api | 矩形/线/填充、脏矩形局部刷新 | day09–10（叠加处理） |
| 24 | ps2-mouse | PS/2 辅助设备中断、三字节包解析、光标绘制 | day07（鼠标） |
| 25 | layer-system | 图层系统：z 序、窗口叠加、区域搬移 | day10（叠加处理） |
| 26 | windows-drag | 窗口结构、标题栏、鼠标拖动窗口 | day11（制作窗口） |
| 27 | desktop-taskbar | 桌面进程：墙纸、任务栏、系统时钟 | day14–18 |
| 28 | gui-terminal | 图形终端窗口：把 day14 的 shell 渲染进窗口 | day17（命令行窗口） |
| 29 | gui-apps | 关于/内存监视/时钟小应用，Alt+Tab 或点击切换 | day22–25（应用） |
| 30 | release | 启动画面、版本徽标、自检报告、发布 ISO 与总验收 | day29–30（润色） |

> 关于"Xfce 桌面"：Linux 0.11 时代的内核无法运行现代 Xfce（缺 framebuffer/网络/现代 ABI）。
> 本课程给出两层满足：① **day21–30 自研图形桌面**（窗口/任务栏/鼠标，30dayMakeOS 式收官）；
> ② `env/` 提供 **Xfce + noVNC 的可复现实验环境**——浏览器即得完整 Xfce 桌面，里面运行 QEMU 图形窗口与本课程工具链，实现"每课打开浏览器就能做实验"。

## 四、可复现性与自动测试

- **构建**：统一 `mk/kernel.mk`（gcc -m32 -ffreestanding、GAS、NASM、grub-mkrescue），每课 `Makefile` 只声明差异。
- **测试**：`tools/qemu-test.sh` 无头启动 QEMU：
  - 串口（COM1）日志做**顺序断言**（`test/expect.txt`）；
  - QMP 截屏（`screendump`）保存 PNG 做人工/像素复核；
  - 内核写 `0xF4` 端口经 `isa-debug-exit` 干净退出，或测试器超时收割。
  - 结果写 `build/test/result.json`，根目录 `make test` 输出全 30 课测试矩阵。
- **环境**：`env/Dockerfile` 固定工具链版本；宿主机版本差异不影响可复现性。
- **历史可查**：`30day-course` 分支上**每课一个 commit**，`git checkout dayNN` 即得当日快照（目录快照 + 提交点双保险）。

## 五、与原仓库内容的关系

- `source/`、`docs/`（模块文档、精读注释、校验脚本）原样保留，继续由 `scripts/check-source-study.py` 与 `source.sha256` 保护。
- 动手轨的代码是**教学重写**（现代工具链 + GRUB + 渐进结构），不是对 0.11 的逐行拷贝；凡借用 0.11 设计/片段（printk、memory.c、sched.c、fork.c、system_call.s、tty 等）都在讲义中注明出处。
