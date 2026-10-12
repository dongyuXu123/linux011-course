/* =====================================================================
 * day07 - kernel/irq_entry.S：IRQ0~15 统一入口(8259 重映射到 0x20~0x2F)
 * 布局与 trap_entry.S 完全一致: [伪错误码][向量号] → 公共出口。
 * ===================================================================== */

.macro IRQSTUB n
irq_stub_\n:
    pushl $0xDEADC0DE          /* IRQ 不带错误码, 补齐布局 */
    pushl $(0x20 + \n)
    jmp  irq_common
.endm

.section .text
IRQSTUB 0
IRQSTUB 1
IRQSTUB 2
IRQSTUB 3
IRQSTUB 4
IRQSTUB 5
IRQSTUB 6
IRQSTUB 7
IRQSTUB 8
IRQSTUB 9
IRQSTUB 10
IRQSTUB 11
IRQSTUB 12
IRQSTUB 13
IRQSTUB 14
IRQSTUB 15

irq_common:
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
    pushl %esp
    call do_irq                /* C: 按 regs->vector 分发 + 发 EOI */
    addl $4, %esp
    popl %gs
    popl %fs
    popl %es
    popl %ds
    popa
    addl $8, %esp
    iret

.section .rodata
.global irq_stub_table
irq_stub_table:
.irp i, 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
    .long irq_stub_\i
.endr
