# day14 - kernel-shell：行编辑与内置命令 shell

第一周收官：把前面所有子系统拧成一个交互界面。`kmain` 进入永久循环——提示符 `linux011> `、`tty_getc()` 逐字符收行（退格删字、忽略 `\r`、回车收行），对整行做命令分发：`help` 列内置命令、`ps` 打印任务表（遍历 day11/12 的 `task[]`）、`uptime` 用 jiffies 折算秒数、`clear` 清屏、`echo` 回显参数。0.11 没有内核内置 shell（它的 sh 是用户态程序 `/bin/sh`），本课是对那个交互形态的内核态预演，也为 day16+ 的文件命令、day28 的图形终端搭好骨架。

测试经 QMP 注入 `ps⏎`、`uptime⏎`、`help⏎` 三组按键。串口输出节选：

```
[day14] linux011 mini shell -- type 'help'
pid  state  name
uptime: N.NN s (jiffies NNN)
builtins: help ps uptime clear echo
```

## 知识点

- **行编辑的两层职责**：`con_putc('\b')` 负责屏幕上的擦除（day05 实现的退格分支），shell 的收行循环负责缓冲区里的 `n--`——显示层与逻辑层各管一半，这在 0.11 里是 tty 规范模式（`read_chan`）与终端驱动（`con_write`）的分工。
- **ps 命令 = 任务表的自省**：直接遍历 `task[NR_TASKS]` 打 `pid/state/name`。state 三态 `run/zomb/dead` 对应 day12 的 `TASK_RUNNABLE/ZOMBIE/DEAD`——一条命令把 day11/12 的数据结构变成可观察对象。
- **uptime 与 jiffies**：`jiffies/100` 得秒、`jiffies%100` 得百分秒（PIT 固定 100Hz）。0.11 同名宏 `jiffies` 在 `timer_interrupt` 里递增——两边的"系统时间"概念完全一致，只是这里没有 `mktime`/实时钟。
- **命令分发的朴素实现**：`strcmp` 链 + 前缀匹配（`strncmp("echo ",5)`）。真实 shell 要做词法切分，课程内核在 `echo` 上用一个空格切参数已够演示"内置命令 vs 外部命令"的分野——day16/17 的 `cat/write/run` 会沿用这套骨架。
- **为何 shell 放内核里**：0.11 的 shell 需要进程、文件系统与 `/bin/sh` 可执行文件——那些分别在 day13（有 ring3 了）、day15（文件系统）、day17（exec）才到位。先用内核态内置命令拿到可交互系统，之后"shell 外壳化"有清晰的对照物。
- **unknown command 的教学价值**：分发落到兜底分支时打印 `unknown command: %s (try 'help')`——交互系统的第一可用性原则是"告诉用户还能干什么"。

## 关键代码走读

（本课路径均相对 `days/day14-kernel-shell/`）

`kernel/main.c` 的收行循环 —— 行编辑核心：

```c
printk("linux011> ");
for (;;) {
    char c = tty_getc();
    if (c == '\n') { line[n] = 0; con_putc('\n'); break; }
    if (c == '\b') { if (n > 0) n--; continue; }
    if (c == '\r') continue;
    if (n < 63) line[n++] = c;
}
if (line[0]) do_cmd(line);
```

`kernel/main.c` 的 `cmd_ps` —— 任务表自省：

```c
static const char *st_name[] = { "run", "zomb", "dead" };
printk("pid  state  name\n");
for (int n = 0; n < NR_TASKS; n++)
    printk("%-4d %-6s %c\n", task[n].pid, st_name[task[n].state], task[n].name);
```

`kernel/main.c` 的 `do_cmd` 分发头：

```c
if (!strcmp(line, "help"))   { cmd_help(); return; }
if (!strcmp(line, "ps"))     { cmd_ps(); return; }
if (!strncmp(p, "echo ", 5)) { printk("%s\n", p + 5); return; }
printk("unknown command: %s (try 'help')\n", line);
```

## 验收

`test/expect.txt` 的断言（`Makefile` 的 `KEYS := p,s,ret,u,p,t,i,m,e,ret,h,e,l,p,ret` 注入三组命令）：

1. `linux011 mini shell -- type 'help'` —— shell 启动；
2. `pid  state  name` —— `ps` 命令被正确解析并走通任务表打印（表头随命令而现，说明不是启动横幅的重复）；
3. `uptime: ` —— `uptime` 命令把 jiffies 折算出可读时长（数字部分不断变化，故只断前缀）；
4. `builtins: help ps uptime clear echo` —— `help` 输出的命令清单，同时是本课功能面的自描述。

四条断言之外，`ps` 输出的任务表体（`0 run 0` 等）因调度状态随时间变化，不做逐行断言——`pid state name` 表头的出现即是命令执行成功的标志。

测试是端到端的：QMP 按键 → 键盘中断 → tty 队列 → 行编辑 → 命令分发 → printk。无像素断言。

## 对照阅读

- `source/kernel/chr_drv/tty_io.c` 的 `read_chan()`：0.11 的行编辑（退格 `\b` 处理、回车唤醒）发生在 tty 规范模式的队列层，而不是 shell 里——课程把行编辑放 shell、擦屏放 con 层，分层位置不同但职责一致。
- 0.11 的 `/bin/sh` 不在源码树里（用户态镜像烧在根文件系统上）；`init/main.c` 的 `init()` 里 `execve("/bin/sh",...)` 是它的调用点——day19 的用户态 init 会真正跑这一步。
- `source/kernel/sched.c` 的 `task[]` 与 `NR_TASKS`：`ps` 打印的正是它的课程缩影。

## 实验

1. **加一个 history 命令**：用静态缓冲保存最近 4 行，`history` 逆序打印；注意缓冲区管理要用环形（想想 day08 的 `read_q`）。
2. **区分 `\r` 与 `\n`**：把收行循环里 `if (c=='\r') continue;` 改成把 `\r` 也当行结束，再用 QMP 注入一组含 `\r` 的按键，观察行为差异——理解 0.11 tty 的 `INLCR/ICRNL` 标志到底在转换什么。
