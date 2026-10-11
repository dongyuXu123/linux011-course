# day16 - ramfs-files：文件读写 API 与 shell 的 cat/write

给文件系统补上"读写通路"。`fs/ramfs.c` 的 `ramfs_read/ramfs_write` 上课已就位，本课把它们暴露成 shell 命令：`cat <file>` 经 read API 把文件内容搬进内核缓冲再打印（不存在时报错）；`write <file> <text...>` 先 `ramfs_create`（已存在则校验 `ramfs_size`），再整体覆写并报告写入字节数。至此"创建-写-读-列"的文件生命周期闭环，用户可见的文件系统成型。

测试注入三组按键：`cat welcome.txt⏎`、`write note hi from day16⏎`、`cat note⏎`。串口输出：

```
[day16] linux011 mini shell
wrote 13 bytes to note
hi from day16
```

## 知识点

- **cat 的最小实现**：`ramfs_read(name, catbuf, sizeof(catbuf)-1)` 一次读全文件，返回值 <0 表示文件不存在——与 0.11 `file_dev.c` 的 `file_read` 相比省掉了"按 `f_pos` 逐块读"的偏移机，但"读端拿字节数、补结尾、防越界"的姿势相同。
- **write 的"创建或校验"两段式**：`ramfs_create(args) < 0 && ramfs_size(args) < 0` 才算失败——create 拒绝说明既创建不了（槽满/重名）也无法确认旧文件存在。0.11 里这对应 `open(O_CREAT)` 与 `write()` 两个系统调用的组合，本课把两步折叠进一条 shell 命令。
- **覆写语义**：`ramfs_write` 是整文件替换（`memcpy` 后 `f->size = len`），没有追加与随机写。0.11 `file_write` 按 `f_pos` 定位、逐块经缓冲层写设备并按需扩块（`fs/bitmap.c` 的 `new_block`）——覆写是它的 `len == f_size` 特例。
- **read 的截断返回值**：`ramfs_read` 把过长的请求压缩到文件实际大小并返回真实字节数，调用方 `catbuf[n]=0` 后按字符串打印——"返回值即有效长度"是 read/write API 的核心契约，day19 用户态的 `read` 系统调用沿用同一约定。
- **shell 的成长方式**：`do_cmd` 只加了两行 `strncmp` 分发，`cmd_cat/cmd_write` 各 ~15 行。对比 0.11：这些功能在用户态由 `cat` 等 bin 程序经文件系统调用完成——内核态内置命令与用户态工具的差异，会在 day17（exec）与 day19（真用户程序）逐步收窄。
- **`help` 自描述**：命令清单字符串 `"builtins: help ps uptime clear echo ls cat write"` 同时是文档与测试断言物——每加一条命令改一处字符串，验收面自动跟上。

## 关键代码走读

（本课路径均相对 `days/day16-ramfs-files/`）

`kernel/main.c` 的 `cmd_cat` —— 经 read API 读文件：

```c
static char catbuf[2048];
static void cmd_cat(const char *name)
{
    int n = ramfs_read(name, catbuf, sizeof(catbuf) - 1);
    if (n < 0) { printk("cat: %s: no such file\n", name); return; }
    catbuf[n] = 0;                    /* 读多少补多少结尾 */
    printk("%s", catbuf);
}
```

`kernel/main.c` 的 `cmd_write` —— 创建或校验后整文件覆写：

```c
char *sp = strchr(args, ' ');         /* 文件名与文本以第一个空格分隔 */
if (ramfs_create(args) < 0 && ramfs_size(args) < 0) {
    printk("write: cannot create %s\n", args);
    return;
}
int n = ramfs_write(args, sp + 1, strlen(sp + 1));
printk("wrote %d bytes to %s\n", n, args);
```

`fs/ramfs.c` 的 `ramfs_read` —— 截断到文件尾：

```c
if (len > f->size) len = f->size;
memcpy(buf, f->data, len);
return len;
```

## 验收

`test/expect.txt` 四行顺序断言（对应注入的 `cat welcome.txt` / `write note hi from day16` / `cat note`）：

1. `linux011 mini shell` —— shell 就绪；
2. `wrote 13 bytes to note` —— `write` 命令把 `"hi from day16"`（恰好 13 字符）写入新文件 `note`，字节数记账精确；
3. `hi from day16` —— 紧随的 `cat note` 把刚写的内容原样读回：**写读闭环**在同一测试里完成，任何 write/read 不对称都会让这行对不上；
4. `cat welcome.txt` 读出的预置内容行（`welcome to ...`）在日志更早处被顺序消费。

本课无像素断言。

## 对照阅读

- `source/fs/file_dev.c` —— `file_read/file_write`：以 `inode`+`filp` 的 `f_pos` 为轴、经 `fs/buffer.c` 的块缓冲读写设备；本课的 `memcpy` 版是"设备为内存"的特例。
- `source/fs/read_write.c` —— `sys_read/sys_write` 系统调用层：fd → `filp` → 调 `file_read/write`。day19 的 `sys_read`（用户态 init 的 read）会以它为对照。
- `source/fs/open.c` 的 `sys_open` + `O_CREAT` 分支：`open_namei → dir_namei → find_entry` 的名字解析链——本课 `ramfs_lookup` 的一层线性扫描是它的最简替身。
- `source/fs/namei.c` 的 `find_entry`：0.11 在目录文件里逐 dirent 比对文件名，与 `ramfs_lookup` 的语义对照。

## 实验

1. **实现追加写**：给 `ramfs_write` 加一个 `at` 参数（或新增 `ramfs_append`），`write` 命令带 `-a` 时追加而不是覆写；测 `write note x` 两次后 `cat` 应看到两段拼接。
2. **防御式测试**：注入 `cat nosuch`（QMP 按键序列里拼出来），确认 `no such file` 报错不崩内核；再把 `catbuf` 缩到 16 字节，观察长文件被截断——理解为什么 0.11 的 read 循环要以 `f_pos` 分多次读。
