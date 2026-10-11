# day04 - grub-multiboot：GRUB 接管引导，第一个 C 内核

从今天起不再手写 bootsect/setup：**GRUB 2 接管引导**。它负责实模式初始化、内存探测、把内核按 Multiboot 规范装到内存、切好保护模式，然后跳到我们的 `_start`，并在寄存器里留下交接凭据（EAX=魔数、EBX=信息块地址）。本课首次用 C 写内核（`kernel/main.c`，freestanding 无 libc），校验 Multiboot 魔数、打印 GRUB 探测到的内存大小，最后经 QEMU 的 `isa-debug-exit` 设备干净退出——这套"跑完自动退出"从此成为每课测试的收尾方式。

构建链也换成现代流程：`gcc -m32` 编译 + `ld` 链接出 `kernel.elf`，`objcopy -O binary` 转平坦镜像，`grub-mkrescue` 打包成可启动 ISO（`IMG=iso`）。串口输出：

```
[day04] booted by GRUB, entered kmain
[day04] multiboot magic ok, info at 0x...
[day04] mem_lower=...KB mem_upper=...KB
[day04] exiting qemu via port 0xF4
```

## 知识点

- **Multiboot 交接约定**：GRUB 跳入 `_start` 时 `EAX = 0x2BADB002`（魔数），`EBX = multiboot_info` 结构的物理地址。信息块是 GRUB 留在内存里的"交接清单"，`flags` 各位声明哪些字段有效——本课用 bit0（`mem_lower`/`mem_upper`，单位 KB）。角色相当于 0.11 里 `setup.s` 收集后放在 `0x90000` 的参数块。
- **Multiboot 头**：内核镜像开头必须有一个 12 字节头（magic/flags/checksum，checksum = −(magic+flags)），GRUB 据此认出"这是我能加载的内核"。flags bit0 要求模块 4K 对齐、bit1 要求内存信息。
- **AOUT_KLUDGE 平坦镜像加载**：课程头置 flags bit16（AOUT_KLUDGE），并附 5 元组 `header_addr/load_addr/load_end/bss_end/entry`。GRUB 据此按"文件偏移 = header_addr − load_addr"的**平坦镜像**方式装载，并把 `[load_end, bss_end)` 清零——`.bss` 清零是 C 运行环境的前提，也是镜像里有 ELF 之外的强约束的原因。day04 起 GRUB 全程按这条 aout-kludge 路径加载课程内核。
- **freestanding C**：`-ffreestanding -nostdlib -fno-pic`，没有 libc 就没有运行时：`kmain` 之前必须先有栈（`multiboot.S` 里 `.skip 16384` 的 16KB 栈），`outb/inb` 用内联汇编自造，字符串打印用裸 `while(*s)` 循环。
- **isa-debug-exit 退出通道**：QEMU 设备 `isa-debug-exit`（`-device isa-debug-exit,iobase=0xf4`）监视 `0xF4` 端口，写入任意值即以 `值*2+1` 为退出码结束 QEMU——课程统一 `outb(0xF4, 60)`（退出码 121），让测试器在超时收割之外多一条确定性收尾路径。

## 关键代码走读

（本课路径均相对 `days/day04-grub-multiboot/`）

`boot/multiboot.S` 的头与 aout-kludge 五元组：

```asm
.set MB_MAGIC,     0x1BADB002
.set MB_FLAGS,     0x3 | 0x10000   /* bit16: AOUT_KLUDGE */
.set MB_CHECKSUM,  -(MB_MAGIC + MB_FLAGS)
.long 0x200000                    /* header_addr = 镜像偏移0 的 VMA */
.long 0x200000                    /* load_addr   = 链接基址 */
.long _kernel_load_end            /* 载入数据结束 */
.long _kernel_end                 /* bss 结束(GRUB 负责清零) */
.long _start                      /* 入口 */
```

`boot/multiboot.S` 的 `_start` —— 先有栈再进 C：

```asm
_start:
    movl $stack_top, %esp     /* 16KB 内核栈 */
    pushl %ebx                /* multiboot_info* */
    pushl %eax                /* 魔数 0x2BADB002 */
    call  kmain
```

`kernel/main.c` 的 `kmain` —— 校验魔数并读内存信息：

```c
if (magic == 0x2BADB002) { ... }
struct multiboot_info *mbi = (struct multiboot_info *)addr;
if (mbi->flags & (1 << 0)) {          /* mem_lower/mem_upper 有效 */
    serial_puts("[day04] mem_lower=");
    serial_puthex(mbi->mem_lower);
```

`mk/kernel.mk`（仓库共享）：`ld -T mk/linker.ld` 链接 → `objcopy -O binary` 出 `kernel.flat` → `grub-mkrescue` 打包 ISO，GRUB 配置由 `mk/grub.head` 与每课 `grub.cfg` 拼接生成。

## 验收

`test/expect.txt` 四行顺序断言：

1. `booted by GRUB, entered kmain` —— GRUB 完整加载并跳入 C 入口（链接脚本、aout-kludge 布局、栈设置任一出错都到不了这里）；
2. `multiboot magic ok` —— EAX 交接凭据正确；
3. `mem_lower=` —— 信息块指针有效、flags bit0 探测成功；
4. `exiting qemu via port 0xF4` —— `isa-debug-exit` 干净退出（`result.json` 里 `qemu_exit=0` 印证）。

本课无像素断言。

## 对照阅读

- `source/boot/bootsect.s` + `source/boot/setup.s`——这两个文件干的活（重定位、探测内存、查光标/显卡参数、进保护模式），今天全部交给了 GRUB；day06 的 `boot/head.S` 从 `_start` 之后接棒，对应 0.11 的 `source/boot/head.s`。
- `source/init/main.c` 开头的 `#define ... ORIG_ROOT_DEV` 等宏，读的就是 0.11 版"multiboot_info"——setup.s 存在 `0x90000` 的参数块。
- Multiboot 规范字段全表见 GRUB 手册 "Boot information format"；后续 day10（mem_upper）、day18/21（mods_addr、framebuffer 字段）都会回来翻它。

## 实验

1. **把魔数校验改成故意失败**：把 `magic == 0x2BADB002` 临时改成 `!=`，确认串口打出 `BAD magic!`——再想想：如果 GRUB 真传错了魔数，哪一步（头？加载？寄存器？）最可能出问题。
2. **再加一个信息块字段**：把 `struct multiboot_info` 补上 `boot_loader_name`（flags bit9 有效），用 `serial_puts((char*)mbi->boot_loader_name)` 打印引导器名字，预期看到 `GRUB2` 之类字符串——顺手熟悉"按 flags 探测字段"的防御式读法。
