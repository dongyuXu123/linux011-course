# day20 - milestone：字符终端里程碑与子系统自检

第一程（day01-20）收官。系统已五脏俱全：GRUB 引导 / 分页 / 中断 / 时钟心跳 / 键盘 tty / 物理页管理 / 任务调度 / fork-exec-wait / int 0x80 / ramfs / 用户态 init。本课做三件事：新增 `kernel/selftest.c` 在启动时跑一轮**子系统自检**并出报告；打出版本横幅 `linux011-course 0.20 (terminal milestone)`；随后照常把 `/sbin/init` 送上 ring3、进入用户态（day19 的全部行为保留）。

自检剧本：物理页子系统做 200 页"分配-写校验字-校验-释放"；时钟子系统确认 jiffies 在走。串口输出（测试随后注入 `run hello⏎` 复验用户态链路）：

```
[day20] selftest begin
[day20] mm: 200-page alloc/verify/free ok
[day20] timer: jiffies ticking
[day20] selftest: all subsystems ok
[day20] linux011-course 0.20 (terminal milestone)
[day19] /sbin/init running in ring 3 (pid 1)
forked child pid 2
[day17] hello world, exec'd from ramfs!
[init] child 2 exited with 0
```

## 知识点

- **自检即回归**：把"每个子系统一条已知输入/输出对"固化成启动时的一次自测。mm 自检的三段（分配 N 页 → 每页写 `0xA5A50000+i` → 读回比对 → 全部释放）同时验证了分配器、内存内容完整性（无越界互踩）与回收。
- **校验字的选择**：每页写**不同**的 pattern（页号编码在低 16 位），才能发现"页 A 的写串到页 B"这类错位；全写同一个值的自检发现不了索引算错。
- **时钟自检的最小形态**：记下 jiffies，`hlt` 睡到它变化即算通过——不校准频率（day07 已验），只验证"中断还活着"。里程碑自检的原则是**快、可断言、不产生副作用**。
- **自检在 Sti 之后、init 之前**：顺序是 `con/trap/irq/timer/sched/syscall/tty/ramfs` 初始化 → `sti` → `selftest_run()` → 加载 init → iret 进 ring3。自检用的页在测完立即 `free_page` 归还，不留泄漏给用户态。
- **0.11 的对应物**：0.11 没有运行时自检，它的"自检"是编译期/启动期的静态断言与 `panic` 网（`memory.c` 里 `if (!p) panic(...)`）。课程把这类检查升级为主动探测——这对 30 天连续增量开发是刚需：任何一天回归全 30 课测试时，自检输出都是第一道哨兵。

## 关键代码走读

（本课路径均相对 `days/day20-milestone/`）

`kernel/selftest.c` 的 `selftest_run` —— mm 三段式：

```c
for (int i = 0; i < POOL_PROBE; i++) {
    pages[i] = get_free_page();
    if (!pages[i]) { ok = 0; break; }
    *(uint32_t *)pages[i] = 0xA5A50000 + i;   /* 每页独立校验字 */
}
for (int i = 0; i < POOL_PROBE && ok; i++)
    if (*(uint32_t *)pages[i] != 0xA5A50000U + i) ok = 0;
for (int i = 0; i < POOL_PROBE; i++)
    if (pages[i]) free_page(pages[i]);
```

`kernel/selftest.c` —— 时钟心跳探测：

```c
uint32_t j0 = jiffies;
while (jiffies == j0)
    __asm__ volatile("hlt");      /* 睡到下一个 tick */
printk("[day20] timer: jiffies ticking (%d)\n", jiffies - j0);
```

`kernel/main.c` —— 自检后交棒 init（day19 逻辑原样保留）：

```c
memset((void *)0x40000, 0, 32768);
ramfs_read("init", (char *)0x40000, 32768);
kernel_task_create(1, launch_init, 'I');
```

## 验收

`test/expect.txt` 九行顺序断言分两组：

自检组：
1. `selftest begin` / `mm: 200-page alloc/verify/free ok` —— 200 页分配、**逐页内容**校验与释放全部通过；
2. `timer: jiffies ticking` —— 时钟中断活着；
3. `selftest: all subsystems ok` + `linux011-course 0.20 (terminal milestone)` —— 版本横幅（里程碑标签本身就是验收物）。

用户态组（day19 链路回归）：
4. `/sbin/init running in ring 3 (pid 1)` → `forked child pid 2` → `hello world, exec'd from ramfs!` → `child 2 exited with 0` —— 确认自检没有对后续系统产生副作用。

无像素断言（本课仍是字符终端形态；图形从 day21 开始）。

## 对照阅读

- `source/init/main.c` —— 0.11 启动收尾的形状：`mem_init/trap_init/blk_init/chr_init/tty_init` 一串 init 后 `move_to_user_mode()`、fork 出 init 进程。本课 `kmain` 的初始化序列 + `launch_init` 与它同构。
- `source/kernel/panic.c` —— 0.11 的失败处理哲学：检查到异常直接 panic 停机；课程自检是"主动体检 + 报告"的互补方案。
- 30dayMakeOS day30：haribote 的收官润色（进度条、版本显示）与本课"里程碑横幅 + 自检"呼应，但课程的自检是真实功能验证。

## 实验

1. **给自检加 shell/调度项**：在 `selftest_run` 里加第三段——fork 一个内核任务、wait 它退出，把 day12 的链路也纳入自检；想想为什么放在 `sti` 之后才有意义。
2. **故意写坏一页**：在校验循环前偷偷改一页内容（`*(uint32_t*)pages[7] = 0;`），观察自检报告 FAILED 与 expect 断言失败——把"自检能拦住回归"这件事亲手触发一遍。
