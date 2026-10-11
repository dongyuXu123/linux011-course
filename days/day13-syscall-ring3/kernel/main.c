/* =====================================================================
 * day13 - kernel/main.c：内核把用户程序送上 ring 3
 * ===================================================================== */
#include <kernel.h>
#include <string.h>

extern char user_blob_start[], user_blob_end[];

static char user_stack[4096] __attribute__((aligned(16)));

/* 内核线程入口: 摆好 ring3 iret 帧, 一脚把用户程序踹进用户态
   (0.11 的 move_to_user_mode 宏干的就是这件事) */
static void user_task_entry(void)
{
    memcpy((void *)0x40000, user_blob_start, user_blob_end - user_blob_start);
    printk("[day13] user program copied to 0x40000, dropping to ring 3\n");
    __asm__ volatile(
        /* 先把数据段寄存器换成用户选择符: iret 到低特权级时 CPU 会
           校验 DS/ES/FS/GS 的 DPL(0.11 move_to_user_mode 同款) */
        "movl $0x23, %eax\n"
        "movl %eax, %ds\n"
        "movl %eax, %es\n"
        "movl %eax, %fs\n"
        "movl %eax, %gs\n"
        "pushl $0x23\n"        /* 用户 ss */
        "pushl $0x7F000\n"     /* 用户 esp(user_stack 不行, 必须用户可映射区) */
        "pushl $0x202\n"       /* eflags: IF=1 */
        "pushl $0x1B\n"        /* 用户 cs (0x18|RPL3) */
        "pushl $0x40000\n"     /* eip */
        "iret");
    for (;;) ;
}

int wait(int *status);

static inline void sti(void)  { __asm__ volatile("sti"); }
static inline void outb(uint16_t p, uint8_t v) { __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p)); }

void kmain(void)
{
    con_init();
    trap_init();
    irq_init();
    timer_init();
    sched_init();
    syscall_init();
    sti();

    printk("[day13] kernel ready, launching user program via int 0x80 ABI\n");
    kernel_task_create(1, user_task_entry, 'U');

    int st = 0;
    int pid = wait(&st);
    printk("[day13] reaped user task pid=%d exit=%d\n", pid, st);
    printk("[day13] syscall ABI ok\n");
    outb(0xF4, 60);
    for (;;)
        __asm__ volatile("hlt");
}
