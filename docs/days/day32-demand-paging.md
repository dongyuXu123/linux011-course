# day32 - 按需调页(0.11 do_no_page)

day31 的 execve 是"预装": 把整个程序映像一次性搬进内存。真 0.11 不是
这样 —— `do_execve` 只登记可执行文件的 inode, 页面等第一次访问触发
#PF 时由 `do_no_page` 从设备按需读入一页; 文件之外的部分(bss/堆)
由零页路径补齐。今天把这个机制装进课程内核: execve 便宜了(0 页拷贝),
页表"按需生长", 与 0.11 `mm/memory.c` 的行为对齐。

## 已实现并验证(test 12/12)

- `task_struct` 新增 `exe_name[16]` / `ex_size` —— do_no_page 的
  "从哪读、读到哪为止"(0.11 里是 inode + i_size)
- `sys_execve` 重写: 清旧映像用户页(栈保留) → 登记映像 → start_thread
  改写 iret 现场。程序页一页都不预装
- `do_no_page(addr, err)`(mm.c): 偏移 < ex_size → `ramfs_read_at`
  从文件读入该页(0.11: bmap+ll_rw_blk 读单页); 偏移 ≥ ex_size →
  零页(get_free_page 天然清零); 两者之外 → 返回 0
- `trap_dispatch`: #PF 且 err bit0=0(缺页而非越权) 先交给
  do_no_page; 补不了就打印坏地址、置 exit_code=139(128+SIGSEGV)、
  标僵尸 —— 课程内核的"段错误"
- `ramfs_read_at(name, off, buf, len)`: 从文件偏移 off 读一段,
  越过文件尾补零(bss) —— 0.11 单页读的对应物

验证链路: fork 出的子任务 execve("hello") → iret 第一条取指即 #PF →
`do_no_page: pid 2 off 0 <- hello`(代码页按需读入) → hello 跑起来 →
`mov byte [0x1000],'Z'` 落在文件之外 → `off 1000 <- zero page`(零页) →
把 'Z' 再打印出来证明页是真实内存 → `bss demand ok: Z` → exit/wait4。

## 关键实现走读

**卸载旧映像时查表要用完整线性地址**(本次踩的坑): 用户页挂在
PDE `slot*16` 起的表里, 直接拿空间内偏移查会误入 0 号表 —— 那是
内核低位恒等映射, 幸好 `free_page` 的地址护栏把误杀全数拒收
(`free_page bad addr ignored` 刷屏就是它), 否则内核自身就被拆了:

```c
uint32_t laddr = TASK_LIN_BASE(slot) + off;   /* 不是 off 本身! */
uint32_t pde = laddr >> 22, pte = (laddr >> 12) & 0x3FF;
```

**do_no_page 的判页逻辑**(对照 0.11 do_no_page 的三岔路):

```c
if (off < current->ex_size)
    ramfs_read_at(current->exe_name, off & ~0xFFF, (char *)phys, 4096);
else                    /* 文件之外: bss/堆, 零页 */
    ;                   /* get_free_page 已清零 */
mm_map_page(current->cr3, addr & ~0xFFF, phys);
```

## 与 0.11 的差异(课程简化)

- 0.11 的程序页来自块设备(buffer cache + bmap), 我们读 ramfs ——
  day33/34 换上块设备层后, 这里会自然长出真路径
- 0.11 do_no_page 还有 share_memory 映射路径; 课程略
- 栈页仍由 execve 预先映射; 0.11 的栈也是按需生长的 —— 留作思考题
- 无 swap: 页只进不出(0.11 同样没有)

## 对照阅读

- `source/mm/memory.c`: do_no_page(共享页/文件页/零页三岔路)
- `source/fs/exec.c`: do_execve → change_ldt —— 只登记不装载
- `source/mm/memory.c`: do_no_page_page 的 zero_page 路径
