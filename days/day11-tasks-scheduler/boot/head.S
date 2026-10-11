/* =====================================================================
 * day06 - boot/head.S：0.11 式 head —— 开启分页，为 C 内核铺路
 * ---------------------------------------------------------------------
 * GRUB 把内核装到 1MB 交给 _start(见 multiboot.S)。head.S 接棒:
 *   1. 内核映像里备好 1 页目录 + 4 张连续页表 —— 恒等映射前 16MB
 *      (0.11 head.s/page.s 同款: pg_dir + pg0..pg3, 4KB 页)
 *   2. cr3 → 页目录, cr0.PG=1 —— 分页开启(恒等映射下代码"原地"运行)
 *   3. call trap_init (C 侧建 IDT) → call kmain
 *
 * 对照阅读: source/boot/head.s + source/mm/page.s
 * ===================================================================== */

.section .bss
.align 4096
.global page_dir
page_dir:
    .skip 4096
pg_table0:                     /* 4 张页表连续排布, 一次性顺序填充 */
    .skip 4096
pg_table1:
    .skip 4096
pg_table2:
    .skip 4096
pg_table3:
    .skip 4096

.section .text
.global head
head:
    cld
    cli

    /* ---- 顺序填 4096 个页表项: 项 i = 页帧 i | P|R|U ---- */
    movl $pg_table0, %ebx
    xorl %ecx, %ecx
1:  movl %ecx, %eax
    shll $12, %eax             /* 页帧物理地址 = 页号 * 4KB */
    orl  $0x7, %eax            /* present|rw|user */
    movl %eax, (%ebx)
    addl $4, %ebx
    incl %ecx
    cmpl $4096, %ecx           /* 4 表 × 1024 项 */
    jb 1b

    /* ---- 页目录: 低 4 项指向 4 张页表, 其余清零 ---- */
    movl $page_dir, %edi
    movl $1024, %ecx
2:  movl $0, (%edi)
    addl $4, %edi
    loop 2b

    movl $pg_table0, %eax
    orl  $0x7, %eax
    movl %eax, page_dir + 0
    movl $pg_table1, %eax
    orl  $0x7, %eax
    movl %eax, page_dir + 4
    movl $pg_table2, %eax
    orl  $0x7, %eax
    movl %eax, page_dir + 8
    movl $pg_table3, %eax
    orl  $0x7, %eax
    movl %eax, page_dir + 12

    /* ---- cr3 与 cr0.PG: 正式开启分页 ---- */
    movl $page_dir, %eax
    movl %eax, %cr3
    movl %cr0, %eax
    orl  $0x80000000, %eax
    movl %eax, %cr0
    jmp 3f
3:
    /* ---- 装载自己的 GDT: GRUB 的选择符是 CS=0x10/DS=0x18，
       与课程约定(0x08/0x10, 0.11 同款)不同，必须换成自己的 ---- */
    lgdt gdtdesc
    ljmp $0x08, $reload
reload:
    movl $0x10, %eax
    movl %eax, %ds
    movl %eax, %es
    movl %eax, %fs
    movl %eax, %gs
    movl %eax, %ss

    call trap_init
    call kmain
4:  hlt
    jmp 4b

/* 与 day03 相同的 GDT 布局: 0x08 代码 / 0x10 数据, 基址0 4GB */
.align 8
gdt:
    .quad 0
    .quad 0x00CF9A000000FFFF
    .quad 0x00CF92000000FFFF
gdtdesc:
    .word gdt_end - gdt - 1
    .long gdt
gdt_end:
