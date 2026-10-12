# fork 的 asm 包装: 精确拿到"调用者的 esp"(即 fork 返回地址所在槽),
# 子进程的复制与恢复帧都以此为基准 —— 保证子进程 ret 后正好回到
# 父代码 fork() 调用点的下一条指令, 且 EAX=0。
.text
.global fork
fork:
    pushl %ebp
    pushl %ebx
    pushl %esi
    pushl %edi
    movl  %esp, %eax
    addl  $16, %eax          # eax = 调用者的 esp(栈顶是 fork 的返回地址)
    pushl %eax
    call  do_fork            # C: 完成槽位分配/栈复制/恢复帧
    addl  $4, %esp
    popl  %edi
    popl  %esi
    popl  %ebx
    popl  %ebp
    ret                      # 父进程带着子 pid 返回; 子进程不走这里
