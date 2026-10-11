# =====================================================================
# day11 - kernel/switch.S：软件上下文切换
# ---------------------------------------------------------------------
# 0.11 用 ljmp-to-TSS 硬件切换(讲义里详解原理); 课程实现改用等价的
# 软件切换: 每个任务挂起时, 内核栈上留有 [返回点][edi][esi][ebx][ebp],
# 调度器交换 esp 即完成任务切换 —— 与 Linux 现代实现同构。
# =====================================================================
.text
.global cpu_switch_to
# void cpu_switch_to(uint32_t *prev_esp_out, uint32_t next_esp)
cpu_switch_to:
    pushl %ebp
    movl  %esp, %ebp
    pushl %ebx
    pushl %esi
    pushl %edi

    movl  8(%ebp), %ecx       # prev_esp_out
    pushl $1f                 # 恢复点: 回到本函数尾部弹出寄存器
    movl  %esp, (%ecx)        # 保存当前任务的栈指针(含完整挂起现场)

    movl  12(%ebp), %esp      # 切到下一个任务的栈
    ret                       # 弹出它的恢复点

1:                            # 被调度回来的任务从这里继续
    popl %edi
    popl %esi
    popl %ebx
    popl %ebp
    ret

# 新任务首次入口: 若是从时钟中断里被切进来的, 继承的是关中断状态,
# 必须先 sti 才能被时钟再次抢占; ret 落到初始帧里的 fn。
.global task_entry_stub
task_entry_stub:
    sti
    ret
