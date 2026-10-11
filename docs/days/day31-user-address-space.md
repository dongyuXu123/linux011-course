# day31 - 每进程地址空间(0.11 式, WIP)

## 已实现并验证
- 每任务独立页目录: `mm_pgdir_new()` 复制引导页目录(内核 16MB 恒等 + LFB 映射随之共享)
- 64MB/任务线性布局: `TASK_LIN_BASE(n) = n*64MB`, LDT 每 任务(代码/数据段, DPL3)
- CR3/LDTR 随任务切换: `schedule()` 中 `mm_load_cr3/mm_load_ldt`
- `mm_map_page` 按需挂物理页; fork 的 `copy_mem` 逐页复制; execve 线性装载
- 用户指针翻译 `UPTR`(段内偏移 → 内核线性) 与内核缓冲纪律(cli 拷贝, 防抢占换 CR3)
- 已验证: init 以 ring3 在自己的 64MB 空间运行, banner 经 sys_write 输出; fork 出子任务

## 已知遗留 bug(下一课修复)
fork 后的子任务调用 execve 装载 hello 后, 子任务用户态执行落到
base+0x1fff(装载镜像的零填充尾部)而不是入口 0 —— 即 execve 的
`regs->eip=0` 未生效或被覆盖。GDB 实测: execve 框架 eip=0x59/esp/CS
在装载前正确, 装载内容 v0=0x4b8 正确; IRET 后子任务 eip=0x1fff。
怀疑方向: fork 复制帧与 execve 新帧在内核栈顶重叠(task_setup_mm 后
do_fork 复制区与新 int-0x80 帧的相互作用)。
