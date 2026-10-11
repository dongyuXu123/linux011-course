/* =====================================================================
 * day04 - boot/multiboot.S：Multiboot 规范入口，GRUB 接管引导
 * ---------------------------------------------------------------------
 * 从今天起不再手写 bootsect/setup：GRUB 负责实模式初始化、内存探测、
 * 把内核 ELF 装到 1MB 处、切好保护模式，然后跳到 _start，约定：
 *     EAX = 0x2BADB002 (multiboot 魔数)   EBX = multiboot_info 物理地址
 *
 * 头部三个 long 依次是 magic/flags/checksum，checksum = -(magic+flags)。
 * flags bit0: 模块按 4K 对齐    bit1: 要求内存信息(mem_lower/upper)
 *
 * 对照阅读: source/boot/bootsect.s + source/boot/setup.s —— 这两个文件
 * 干的活，今天全部交给了 GRUB；day06 的 head.s 从 _start 之后接棒。
 * ===================================================================== */

.set MB_MAGIC,     0x1BADB002
.set MB_FLAGS,     0x3 | 0x10000   /* bit16: AOUT_KLUDGE —— GRUB 据此用
                                      平坦镜像方式加载, 并支持 module */
.set MB_CHECKSUM,  -(MB_MAGIC + MB_FLAGS)

.section .multiboot, "a"
.align 4
.long MB_MAGIC
.long MB_FLAGS
.long MB_CHECKSUM
/* AOUT_KLUDGE 五元组: GRUB 按"文件偏移=header_addr-load_addr"平坦加载,
   并把 [load_end, bss_end) 清零(页表/内核静态变量依赖这一点) */
.long 0x200000                    /* header_addr = 镜像偏移0 的 VMA */
.long 0x200000                    /* load_addr   = 链接基址 */
.long _kernel_load_end            /* 载入数据结束 */
.long _kernel_end                 /* bss 结束 */
.long _start                      /* 入口 */

.section .bss
.align 16
stack_bottom:
.skip 16384                   /* 16KB 内核栈 */
stack_top:

.section .text
.global _start
.type _start, @function
_start:
    movl $stack_top, %esp     /* 先有栈才能跑 C */

    pushl %ebx                /* multiboot_info* */
    pushl %eax                /* 魔数 0x2BADB002 */
    call  kmain
    /* kmain 不该返回；返回就关机挂起 */
2:  cli
    hlt
    jmp 2b
.size _start, . - _start
