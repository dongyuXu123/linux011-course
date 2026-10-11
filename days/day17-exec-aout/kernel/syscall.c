/* =====================================================================
 * day13 - kernel/syscall.c：系统调用分发(0.11 sys_call_table 思想)
 * 调用号沿用 0.11 include/unistd.h: exit=1 write=4 getpid=20
 * ===================================================================== */
#include <kernel.h>

#define __NR_exit   1
#define __NR_write  4
#define __NR_getpid 20

static int sys_write(int fd, const char *buf, int len)
{
    if (fd != 1 || len <= 0 || len > 4096)
        return -1;
    for (int i = 0; i < len; i++)
        con_putc(buf[i]);      /* 同地址空间, 直接读用户缓冲 */
    return len;
}

static int sys_getpid(void)
{
    return current->pid;
}

static int sys_exit(int code)
{
    printk("[day13] user task called exit(%d)\n", code);
    current->exit_code = code;
    current->state = TASK_ZOMBIE;
    schedule();                /* 不再回来, iret 永远不会发生 */
    return 0;
}

int syscall_dispatch(struct pt_regs *regs)
{
    switch (regs->eax) {
    case __NR_write:  return sys_write(regs->ebx, (const char *)regs->ecx, regs->edx);
    case __NR_getpid: return sys_getpid();
    case __NR_exit:   return sys_exit(regs->ebx);
    default:
        printk("[day13] unknown syscall %d (ebx=%x ecx=%x vec=%x eip=%x cs=%x)\n",
               regs->eax, regs->ebx, regs->ecx, regs->vector, regs->eip, regs->cs);
        return -1;
    }
}

/* 系统 TSS: GDT 0x28 槽描述符由这里回填(head.S 里只是占位) */
static void tss_install(void)
{
    extern char gdt[], system_tss[];
    uint32_t base = (uint32_t)(uintptr_t)system_tss;
    uint32_t lim  = 103;                     /* 32位 TSS 最小 104 字节 */
    uint8_t *d = (uint8_t *)gdt + 5 * 8;
    d[0] = lim & 0xFF;        d[1] = (lim >> 8) & 0xFF;
    d[2] = base & 0xFF;       d[3] = (base >> 8) & 0xFF;
    d[4] = (base >> 16) & 0xFF;
    d[5] = 0x89;              /* present, DPL0, 32-bit available TSS */
    d[6] = (lim >> 16) & 0x0F;
    d[7] = (base >> 24) & 0xFF;

    uint32_t *t = (uint32_t *)system_tss;
    t[0] = 0;                 /* back link */
    t[1] = 0;                 /* esp0: 每次任务切换时更新(sched.c) */
    t[2] = 0x10;              /* ss0 */
    __asm__ volatile("ltr %%ax" : : "a"((uint16_t)0x28));
}

/* int 0x80 门: DPL=3 的中断门(0xEF), 用户态可触发 */
void syscall_init(void)
{
    tss_install();
    extern char system_call[];   /* kernel/syscall.S */
    trap_install_gate(0x80, (uint32_t)(uintptr_t)system_call);
    extern struct idt_entry idt[256];
    idt[0x80].type_attr = 0xEF;   /* DPL3: ring3 可以 int 0x80 */
}
