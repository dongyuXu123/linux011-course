# day06 - paging-idt：0.11 式 head——开分页、装 IDT、接住异常

本课照着 0.11 的 `head.s` 写出课程版 `boot/head.S`：GRUB 把内核装到 1MB 交给 `_start` 之后，head 接棒——在内核映像里备好 1 页目录 + 4 张连续页表，**恒等映射前 16MB**，置 `cr3` 与 `CR0.PG` 开启分页；再重装自己的 GDT，调用 C 侧 `trap_init()` 建好 IDT。最后 `kmain` 用 `int $0` 主动触发 0 号异常，演示异常经 IDT 进内核、处理、`iret` 返回的全过程。

串口输出：

```
[day06] head.S done: paging enabled
[day06] CR0.PG=1, page_dir at 0x...
[day06] raising vector 0 via int $0 ...
[day06] exception 0x00000000 (#DE divide error) handled, returning
[day06] returned from #DE handler via IDT
[day06] all done
```

## 知识点

- **恒等映射**：页表把虚拟地址 0~16MB 一对一映射到相同物理地址，所以开分页后代码"原地"继续跑，不需要任何地址搬迁。0.11 的 `head.s`+`mm/page.s` 就是这 5 张表（`pg_dir` + `pg0..pg3`）映射 16MB；本课的 `page_dir + pg_table0..3` 同款。
- **页表批量填充技巧**：4 张页表连续排布，可以当一张 4096 项的大表顺序填——项 i = 页帧 i×4KB | P|R|U（`0x7`）。页目录只填低 4 项指向它们，其余 1020 项清零（未映射访问将触发 #PF）。
- **为什么必须重装 GDT**：GRUB 留下的选择符是它自己的布局（CS=0x10、DS=0x18），与课程约定（0x08 代码/0x10 数据，0.11 同款）不同。`lgdt` + `ljmp $0x08, $reload` + 重装 6 个数据段寄存器，把"段描述符的所有权"从 GRUB 收回来。
- **IDT 与中断门**：256 项 `idt_entry`，每项记录处理程序偏移、选择符 0x08 与 `type_attr=0x8E`（P=1、DPL0、32 位中断门）。`lidt` 指向它之后，向量 0~31（CPU 异常）都有处可去。
- **错误码对齐**：8 个异常（#DF/#TS/#NP/#SS/#GP/#PF 等）由 CPU 自动压错误码，其余不压。统一栈布局的办法：不压的补一个占位错误码 `0xDEADC0DE`，再由 stub 压入向量号——出口统一 `addl $8` 弹掉两格后 `iret`。0.11 的 `kernel/asm.s` 就是这个思路的"每异常一段手写"版本。
- **fault 与 trap 的演示细节**：#DE（除零）是 fault，真实除法出错 `iret` 后会重执行出错指令、无限循环；`int $0` 是软件陷阱，压入的 EIP 是**下一条**指令，所以处理完能直接返回继续。`kmain` 选 `int $0` 正是绕开"修复现场"这个尚未实现的能力。

## 关键代码走读

（本课路径均相对 `days/day06-paging-idt/`）

`boot/head.S` —— 4096 个页表项一次顺序填完：

```asm
movl $pg_table0, %ebx
xorl %ecx, %ecx
1:  movl %ecx, %eax
    shll $12, %eax             /* 页帧物理地址 = 页号 * 4KB */
    orl  $0x7, %eax            /* present|rw|user */
    movl %eax, (%ebx)
    ...
    cmpl $4096, %ecx           /* 4 表 × 1024 项 */
```

`boot/head.S` —— 开分页并重装 GDT：

```asm
movl $page_dir, %eax
movl %eax, %cr3
movl %cr0, %eax
orl  $0x80000000, %eax        /* CR0.PG */
movl %eax, %cr0
...
lgdt gdtdesc
ljmp $0x08, $reload
call trap_init
call kmain
```

`kernel/trap_entry.S` 的 `stub_common` —— 统一现场与分发：

```asm
stub_common:
    pusha
    pushl %ds / %es / %fs / %gs
    pushl %esp                 /* 参数: 栈上现场 = struct pt_regs* */
    call  trap_dispatch
    ...
    addl $8, %esp              /* 弹掉 向量号+错误码 */
    iret
```

`kernel/traps.c` 的 `trap_init` —— 填 256 项并 `lidt`；`trap_dispatch` 按 `regs->vector` 打印异常名（"#DE divide error" 等）。

## 验收

`test/expect.txt` 六行顺序断言：

1. `head.S done: paging enabled` + `CR0.PG=1, page_dir at ` —— 分页确实开启且 `cr3` 指向我们的页目录；
2. `raising vector 0 via int $0 ...` —— 演示开始；
3. `exception 0x00000000 (#DE divide error) handled, returning` —— IDT 门 → stub 压栈 → C 分发的整条链路工作正常；
4. `returned from #DE handler via IDT` + `all done` —— `iret` 返回路径正确（栈帧平衡，错一格就跑飞）。

本课无像素断言。

## 对照阅读

- `source/boot/head.s` —— 本课的直接原型：同样的 `pg_dir`+4 页表（0.11 在 `mm/page.s` 里展开）、`cr3/cr0` 两行开分页、重装段寄存器后 `lss stack_start` 设栈、`jmp after_page_fault` 等。差别：0.11 开分页后立刻检查 math coprocessor 并做 `jmp 1f` 校验，课程版把这些省了。
- `source/kernel/traps.c` —— 0.11 的 trap 门初始化（`set_trap_gate` 宏在 `include/asm/system.h`，门描述符布局与本课 `idt_entry` 完全一致）与 `do_divide_error` 等处理函数；`source/kernel/asm.s` 对应本课的 `trap_entry.S`。
- 页表项位定义：`source/include/asm/page.h`（P/PWT/PCD/RW/US 位）。

## 实验

1. **访问未映射地址触发真 #PF**：在 `kmain` 里加 `readl(0x03000000)`（16MB 外，页目录为 0），观察异常报告变成 vector 14；再把 `head.S` 页表填充上限改小（比如只映射 8MB），看 8~16MB 的访问也开始 #PF。
2. **真除零**：把 `int $0` 换成 `div` 除零，验证讲义里说的 fault 行为——处理程序返回后重执行同一条 `div`，机器在两行输出之间死循环（测试超时）。想让它"活着返回"，得在 `trap_dispatch` 里把 `regs->eip += 2` 跳过出错指令——这就是"内核修复现场"的第一课。
