# =====================================================================
# day31 - kernel/syscall_entry.S：int 0x80 系统调用入口
# 对照阅读: source/kernel/system_call.s
# =====================================================================
.section .text
.global system_call
system_call:
    pushl $0xDEADC0DE          # 错误码占位(与异常桩同构)
    pushl $0x80                # 向量号
    pusha
    pushl %ds
    pushl %es
    pushl %fs
    pushl %gs
    movl  $0x10, %eax
    movl  %eax, %ds
    movl  %eax, %es
    movl  %eax, %fs
    movl  %eax, %gs
    pushl %esp
    call  syscall_dispatch     # C: 返回值在 eax
    addl  $4, %esp
.global system_call_resume
system_call_resume:            # fork 的子进程从这里"重新入列"回用户态
    movl  %eax, 44(%esp)       # 覆写保存的 eax → 带回用户态
    popl  %gs
    popl  %fs
    popl  %es
    popl  %ds
    popa
    addl  $8, %esp
    iret
