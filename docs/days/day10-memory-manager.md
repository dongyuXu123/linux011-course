# day10 - memory-manager：mem_map 物理页管理与越界/双释放防护

物理内存管理上线。照 0.11 `mm/memory.c` 的思想实现 `kernel/mm.c`：1MB 以上、恒等映射覆盖到的 15MB 按 4KB 分页，`mem_map[]` 字节数组做占用位图；`get_free_page()` 从高位往低位找第一个空闲页、清零后返回；`free_page()` 带范围/对齐/**双重释放**三重防护。`mem_init()` 以 GRUB multiboot_info 报告的 `mem_upper` 封顶可用上限（本环境 32MB 内存的 QEMU 被课程映射策略封在 16MB）。

`kmain` 的验收剧本：分配到耗尽 → 统计空闲页为 0 → 对同一页 free 两次（应被检测）→ 全部归还 → 再分配应成功。串口输出：

```
[day10] paging pool ready, kernel_end=0x...
[day10] pool exhausted after NNNN pages
[day10] free pages now: 0
[day10] double free at 0x... detected!
[day10] re-alloc ok at 0x...
[day10] mm ok
```

## 知识点

- **mem_map 位图**：`PAGING_PAGES = 15MB >> 12 = 3840` 页，每页一个字节（0 空闲/1 占用）。0.11 同样以字节粒度记录（它还用数值区分"共享了几次"），`mem_init()` 里把内核影像占的页标 USED，本课以 `_kernel_end` 4K 对齐为界做同样的事。
- **从高位往低位分配**：`for (i = PAGING_PAGES-1; i >= 0; i--)`。0.11 注释写明动机：高地址页留给"后长出来的结构"（任务结构、页表等会向上生长），低地址留给缓冲。分配顺序是策略而非正确性问题——但一致的方向让碎片行为可预期。
- **get_free_page 的完整契约**：找到页后**必须清零**再交出去（`memset_page`）。0.11 的 `get_free_page` 用 `rep stosl` 干同样的事——fork 复制页表、bss 需求都依赖"新页必净"。
- **双重释放防护**：`free_page` 先查地址合法性（≥`mem_start`、<`mem_end`、4K 对齐），再查 `mem_map[i]==0` 即为 double free——打印诊断并**拒绝**执行。相比"信任调用者"，显式检测让 day12+ 的任务退出路径（可能多次释放）有了安全网。
- **内存上限的来源**：`mem_end = min(1MB + mem_upper×KB, 16MB)`。`mem_upper` 来自 day04 的 multiboot_info——GRUB 探测的真实物理内存；16MB 是恒等映射的上限（day06 只建了 4 张页表），两者取小。
- **saved_mbi 的传递**：`multiboot.S` 在 `_start` 时把 EAX/EBX 存进 `saved_magic/saved_mbi` 全局量——信息块是 GRUB 放在普通内存里的，开分页后（恒等映射）仍可直接读，但必须在它被页分配器重用之前取出需要的字段。

## 关键代码走读

（本课路径均相对 `days/day10-memory-manager/`）

`kernel/mm.c` 的 `mem_init` —— 范围与内核影像占用：

```c
mem_start = start;                     /* 内核影像结束(4K 对齐) */
mem_end = LOW_MEM + PAGING_MEMORY;     /* 封顶 16MB */
uint32_t avail_end = LOW_MEM + (uint32_t)upper_kb * 1024;
if (avail_end < mem_end) mem_end = avail_end;
for (uint32_t a = LOW_MEM; a < start; a += 4096)
    mem_map[(a - LOW_MEM) >> 12] = 1;  /* 内核占的页标已用 */
```

`kernel/mm.c` 的 `get_free_page` —— 0.11 同款方向 + 清零：

```c
for (int i = PAGING_PAGES - 1; i >= 0; i--) {
    if (mem_map[i] == 0) {
        mem_map[i] = 1;
        uint32_t addr = LOW_MEM + ((uint32_t)i << 12);
        memset_page((void *)addr);
        return addr;
    }
}
return 0;                              /* 0 = 无页可分 */
```

`kernel/mm.c` 的 `free_page` —— 双重释放检测：

```c
if (mem_map[i] == 0) {
    printk("[day10] double free at 0x%x detected!\n", addr);
    return;                            /* 拒绝, 不动位图 */
}
```

## 验收

`test/expect.txt` 六行顺序断言：

1. `paging pool ready, kernel_end=` —— 位图初始化与内核影像上界计算正确；
2. `pool exhausted after` —— 分配循环耗尽：耗尽页数 = (16MB − 内核影像)/4KB 量级，断言只看前缀，但数字可人工核对；
3. `free pages now: 0` —— 耗尽判断与 `mem_free_pages()` 统计一致；
4. `double free at` —— 对同一地址第二次 `free_page` 被识别且位图未受污染；
5. `re-alloc ok` —— 全部归还后位图回收干净，能再次分到页；
6. `mm ok` —— 收尾。

本课无像素断言。

## 对照阅读

- `source/mm/memory.c` —— 直接母本：`mem_map[]` 定义、`mem_init(start_mem, end_mem)`、`get_free_page()`（`rep stosl` 清零版）、`free_page()`（0.11 里通过 `page_dir` 检查引用计数的逻辑，课程版退化为位图检查）。注意 0.11 的 `mem_map` 索引 0 对应物理 0，而本课对应 1MB——因为它管理的是"1MB 以上分页内存"。
- `source/mm/page.s` —— 缺页处理 `page_fault`（本课恒等映射全装满，不会触发 #PF；真正的按需分页在 0.11 里服务 `do_no_page` 的写时复制）。
- `source/boot/setup.s`：0.11 的内存大小来自 INT 15H 扩展内存探测，存放在 `0x90002`——与 `mem_upper` 角色相同。

## 实验

1. **打印内存地图**：在 `mem_init` 后加一个循环，按 4KB 粒度统计"内核影像占用/空闲"的分段并 printk 出来，对照 `nm build/kernel.elf | sort` 核对 `_kernel_end`。
2. **制造一次越界释放**：`free_page(LOW_MEM + 4095)`（非对齐）与 `free_page(0)`（低于下界），确认两处都被诊断拒绝——然后回答：为什么"拒绝并打印"比" assert 挂死"更适合课程内核。
