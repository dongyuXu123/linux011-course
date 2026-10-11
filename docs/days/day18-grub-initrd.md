# day18 - grub-initrd：GRUB module 装载 initrd，ramfs 落到引导镜像

用户程序不再焊死在内核里。本课引入 **initrd**：构建时 `tools/mkinitrd.py` 把用户程序打包成自定义的 romfs-lite 格式镜像（循环的 `[4B 名长|名字|4B 数据长|数据]`，补零到 64KB），`Makefile` 通过 `MODULES` 变量把它放进 ISO 的 `/boot/initrd.img`，每课自己的 `grub.cfg` 用 Multiboot 的 `module` 命令让 GRUB 把它装进内存；内核启动后解析 multiboot_info 的 `mods_count/mods_addr` 找到模块，逐个解包成 ramfs 文件。

串口输出（测试随后注入 `run hello⏎`）：

```
[day18] GRUB loaded 1 module(s), initrd at 100000 (146 bytes)
[day18] initrd: added hello (133 bytes)
[day18] initrd: 1 file(s) into ramfs
[day18] linux011 mini shell
run: exec hello at 0x80000
[day17] hello world, exec'd from ramfs!
run: hello (pid 1) exited with 0
```

## 知识点

- **Multiboot module 机制**：grub.cfg 里每条 `module /boot/initrd.img` 让 GRUB 把文件原样载入内存，并在 multiboot_info 的 `mods_addr` 处登记 `multiboot_mod_list {mod_start, mod_end, string, reserved}` 数组。内核读 `flags bit3` 确认有模块，再按 `mod_end - mod_start` 取长度。
- **romfs-lite 打包格式**：`tools/mkinitrd.py` 的极简容器——每文件四元组 `名长|名|数据长|数据` 首尾相接，末尾补零到 64KB。选自定义格式而非 cpio/tar 是教学取舍：解析循环十几行 C 就能写完（0.11 没有对应物，它直接把根文件系统镜像放在软盘第二台）。
- **解析即挂载**：`initrd_load()` 逐文件 `ramfs_create + ramfs_write`——initrd 不是文件系统类型，而是一批"预装文件"；真正提供目录/读写语义的仍是 day15 的 ramfs。Linux 的 `initramfs`（cpio 解包到 rootfs）就是这个模型。
- **命名的延续**：本课起 ramfs 里的 `hello` 来自"盘"而非内核内嵌——day17 `blob.asm` 的 `incbin` 路径退役。串口里 `[day17] hello world...` 字样仍在，那是因为用户程序 `hello.asm` 沿用 day17 的文件与打印前缀（expect 亦按此断言）。
- **这条路径的脆弱性（历史注）**：GRUB 的 module 加载依赖 GRUB 版本行为——本课快照在当前环境实测可用；但 day19 起的源码注释记录了 GRUB2 2.14 对 multiboot1 module 报 out of memory 的经历，课程从 day19 开始改把 initrd 挂在第二块 IDE 盘、由 `kernel/ata.c` 自己读。读本课时请把它当作"引导器装载文件"的教科书案例。

## 关键代码走读

（本课路径均相对 `days/day18-grub-initrd/`）

`grub.cfg` —— module 装载指令：

```
menuentry "linux011-course" {
    multiboot /boot/kernel
    module /boot/initrd.img
    boot
}
```

`kernel/main.c` 的 `initrd_load` —— 找到 GRUB 交来的模块：

```c
struct multiboot_info *mbi = (struct multiboot_info *)saved_mbi;
if (!(mbi->flags & (1 << 3)) || mbi->mods_count == 0) { ...无模块... }
struct multiboot_mod_list *mod = (struct multiboot_mod_list *)mbi->mods_addr;
uint32_t size = mod->mod_end - mod->mod_start;
```

`kernel/main.c` 的 romfs-lite 解包循环：

```c
while (p + 4 <= end) {
    uint32_t nlen = *(uint32_t *)p; p += 4;
    if (nlen == 0 || nlen > 32 || p + nlen + 4 > end) break;
    memcpy(name, p, nlen); name[nlen] = 0; p += nlen;
    uint32_t dlen = *(uint32_t *)p; p += 4;
    ramfs_create(name);
    ramfs_write(name, (char *)p, dlen);
    p += dlen;
}
```

`tools/mkinitrd.py`：`struct.pack("<I", len(nb)) + nb + struct.pack("<I", len(data)) + data`，`PAD = 64*1024`。

## 验收

`test/expect.txt` 的顺序断言：

1. `GRUB loaded 1 module(s), initrd at` —— multiboot_info 的模块链表解析成功（模块数、起始地址来自 GRUB 实报）；
2. `initrd: added hello` —— romfs-lite 四元组解析正确、文件名与长度对上；
3. `initrd: 1 file(s) into ramfs` —— 解包收尾统计一致；
4. `linux011 mini shell` → `run: exec hello at 0x80000` → `[day17] hello world, exec'd from ramfs!` → `run: hello (pid 1) exited with 0` —— 完整链：**盘上文件 → ramfs → exec → ring3 → 退出码回传**。

无像素断言。

## 对照阅读

- `source/fs/super.c` 的 `mount_root()`：0.11 启动时把根设备（软盘镜像）整卷挂上——角色对应本课"把引导介质上的文件集合装进 ramfs"；0.11 没有 initrd 概念（那是 2.4+ 的），它的"initrd"就是**根软盘本身**。
- `source/fs/buffer.c`：0.11 从块设备读文件要经 1KB 块缓冲；本课 initrd 已由 GRUB 放进内存，读取退化为指针扫描。
- `source/boot/setup.s`：0.11 里"引导器交出的东西"是 `0x90000` 参数块 + 根设备号（`ORIG_ROOT_DEV`）；multiboot_info 的 mods 字段是它的现代对位。
- 30dayMakeOS 无对应课（haribote 用 `make install` 把文件写进镜像，运行时经软盘驱动读）——day19 的 ATA 驱动才是它的对照。

## 实验

1. **initrd 里放第二个程序**：改 Makefile 的 `mkinitrd` 参数加 `hello2=...`（先复制一份 `hello.asm` 改打印内容），观察 `added hello2` 后 `run hello2` 也能跑——理解 mod 列表支持多文件。
2. **解析器的鲁棒性**：把 `mkinitrd.py` 的 PAD 改成 0，用尾部零做终止条件重写解包循环，比较两种终止策略的防御性——哪种对"被截断的 initrd"更安全。
