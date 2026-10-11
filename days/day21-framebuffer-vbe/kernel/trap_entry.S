/* =====================================================================
 * day06 - kernel/trap_entry.S：32 个异常的统一入口桩
 * ---------------------------------------------------------------------
 * 有错误码的异常(CPU 自己压栈)与没有的，压栈布局必须统一：
 *   无错误码: [我们压 dummy 错误码][我们压 向量号]
 *   有错误码: [CPU压 错误码]     [我们压 向量号]
 * 出口处统一弹出两格再 iret —— 0.11 kernel/asm.s 就是这个思路的
 * "每异常一段手写"版本。
 * ===================================================================== */

.macro STUB_NOEC n
stub_\n:
    pushl $0xDEADC0DE          /* 占位错误码, 与有码异常对齐 */
    pushl $\n
    jmp  stub_common
.endm

.macro STUB_EC n
stub_\n:
    pushl $\n                  /* 错误码已在栈顶(CPU 压的) */
    jmp  stub_common
.endm

.section .text
STUB_NOEC 0
STUB_NOEC 1
STUB_NOEC 2
STUB_NOEC 3
STUB_NOEC 4
STUB_NOEC 5
STUB_NOEC 6
STUB_NOEC 7
STUB_EC   8
STUB_NOEC 9
STUB_EC   10
STUB_EC   11
STUB_EC   12
STUB_EC   13
STUB_EC   14
STUB_NOEC 15
STUB_NOEC 16
STUB_NOEC 17
STUB_NOEC 18
STUB_NOEC 19
STUB_NOEC 20
STUB_NOEC 21
STUB_NOEC 22
STUB_NOEC 23
STUB_NOEC 24
STUB_NOEC 25
STUB_NOEC 26
STUB_NOEC 27
STUB_NOEC 28
STUB_NOEC 29
STUB_NOEC 30
STUB_NOEC 31

stub_common:
    pusha
    pushl %ds
    pushl %es
    pushl %fs
    pushl %gs
    movl $0x10, %eax
    movl %eax, %ds
    movl %eax, %es
    movl %eax, %fs
    movl %eax, %gs
    pushl %esp                 /* 参数: 现场指针 pt_regs*(栈内基地址) */
    call trap_dispatch
    addl $4, %esp
    popl %gs
    popl %fs
    popl %es
    popl %ds
    popa
    addl $8, %esp              /* 弹掉 向量号+错误码 */
    iret

/* 桩地址表, traps.c 的 trap_init 逐项取用 */
.section .rodata
.global trap_stub_table
trap_stub_table:
.irp i, 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
    .long stub_\i
.endr
