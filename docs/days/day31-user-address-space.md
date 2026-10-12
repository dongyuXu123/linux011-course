# day31 - 每进程地址空间(0.11 式)

今天课程进入阶段二: 把 day12 的"内核线程 + 共享地址空间"升级为
0.11 的真进程模型 —— 每个任务一个独立的 64MB 线性地址空间, fork 复制、
execve 替换。0.11 的三大地基(任务线性布局 task_struct 里的
ldt[]/tss、copy_mem 页复制、sys_execve 里改写 iret 现场)在本课全部落地。

## 已实现并验证(test 9/9)

- 每任务独立页目录: `mm_pgdir_new()` 复制引导页目录(内核 16MB 恒等 + LFB 映射随之共享)
- 64MB/任务线性布局: `TASK_LIN_BASE(n) = n*64MB`, LDT 每任务(代码/数据段, DPL3,
  选择子 USER_CS=0x07 / USER_DS=0x0F)
- CR3/LDTR 随任务切换: `schedule()` 中 `mm_load_cr3`/`mm_load_ldt`
- `mm_map_page` 按需挂物理页; fork 的 `copy_mem` 逐页复制; execve 线性装载
- 用户指针翻译 `UPTR`(段内偏移 → 内核线性, 0.11 get_fs_byte 的对应物)
  与内核缓冲纪律(cli 下拷贝, 防抢占换 CR3)
- 系统调用 ABI 沿用 0.11: exit=1 fork=2 read=3 write=4 execve=11 getpid=20 wait4=72

验证链路: `/sbin/init` 以 ring3 在自己的 64MB 空间运行 → 读入
`run hello` → fork(页复制出子任务) → 子任务 execve 装载 hello →
`[day31] hello world, exec'd from ramfs!` + `my pid is 2` → exit(0) →
init wait4 收割 `[init] child 2 exited with 0`。

## 关键实现走读

**execve = 装载 + 改写 iret 现场**(0.11 sys_execve + start_thread 同法):

```c
/* 装入本任务空间偏移 0, 换栈, 然后直接改写系统调用现场 ——
   本系统调用"返回"时 iret 即落入新程序 */
regs->cs = 0x07;   regs->eip = 0;
regs->ss = 0x0F;   regs->esp = 0xF0000;
regs->ds = regs->es = regs->fs = regs->gs = 0x0F;
regs->eflags = 0x202;
```

选择子 0x07/0x0F 指向 LDT, 而 LDT 基址=本任务 64MB 基址, 于是
eip=0 就是"新程序入口"—— 地址翻译由段机制代劳, 内核不需要知道
程序实际落在哪个线性地址。

## 调试实录(本课最值钱的部分)

execve 后子任务不跑 hello, 而是稳定落在 `eip=0x1fff, cr2=0x8002000,
cs=7, esp=0xeffe0` 的 #PF 风暴。三步定位:

1. **双重翻译**: dispatch 里 `UPTR(regs->ecx)` 传进 sys_write, 函数内部
   又 `UPTR(buf)` 一次 → 0x4000000×2+offset, 第一个崩溃。教训:
   *翻译只做一层, 层间传"空间内偏移"还是"内核线性"要全局约定*。
2. **"重置内核栈"式 execve 不可行**: 从本任务的 int-0x80 处理器里
   重置自己的内核栈顶, 两个压栈字节正好落在 iret 帧的 ss/esp 槽上 →
   IRET 带 ss=0 回 ring3 → #GP 风暴。教训: *execve 必须改写"已保存
   的现场", 不能动"正在执行的栈"* —— 0.11 start_thread 之所以改
   p_regs 而不是切栈, 就是这个原因。
3. **TLB 不自动失效(真根因)**: 子任务执行 init 拷贝期间 TLB 缓存了
   旧页映射; execve 覆写 PTE 并经用户线性地址写入新程序时, 写穿过
   期 TLB 落到**旧物理页**, iret 后按新页表取指拿到的是**全零页**,
   CPU 把零字节当 `add %al,(%eax)` 一路执行到未映射页 —— eip=0x1fff、
   esp 停在 0xEFFE0 全部由此解释。修复: `mm_map_page` 写完 PTE 重载
   CR3(0.11 switch_mm/invlpg 的课程化等价物)。

```c
t[pte] = phys | 0x7;
uint32_t cr3;                        /* 重载当前 CR3 = 全 TLB 刷新 */
__asm__ volatile("movl %%cr3, %0" : "=r"(cr3));
__asm__ volatile("movl %0, %%cr3" : : "r"(cr3));
```

## 对照阅读

- `source/kernel/fork.c`: copy_process → copy_mem(逐页复制)
- `source/kernel/syscall.s` + `system_call.s`: sys_execve → start_thread
  (改 LDT/ldt 基址 + 重置 iret 帧)
- `source/mm/page.s`: switch_pgdir —— CR3 切换即地址空间切换
- `include/linux/sched.h`: task_struct 里 LDT[3] 与 tss 的排布
