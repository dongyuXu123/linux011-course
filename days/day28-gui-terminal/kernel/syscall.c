/* =====================================================================
 * day13 - kernel/syscall.c：系统调用分发(0.11 sys_call_table 思想)
 * 调用号沿用 0.11 include/unistd.h: exit=1 write=4 getpid=20
 * ===================================================================== */
#include <kernel.h>
#include <string.h>

#define __NR_exit     1
#define __NR_fork     2
#define __NR_read     3
#define __NR_write    4
#define __NR_execve   11
#define __NR_getpid   20
#define __NR_wait4    72

int ramfs_read(const char *name, char *buf, uint32_t len);
int  ramfs_size(const char *name);

/* tty 读队列(day08 keyboard.c) */
char tty_q_get(void);
int  tty_q_count(void);

static int sys_read(int fd, char *buf, int len);
static int sys_fork(struct pt_regs *regs);
static int sys_execve(const char *name, struct pt_regs *regs);
static int sys_wait4(int pid, int *status);

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
    case __NR_write:   return sys_write(regs->ebx, (const char *)regs->ecx, regs->edx);
    case __NR_getpid:  return sys_getpid();
    case __NR_exit:    return sys_exit(regs->ebx);
    case __NR_read:    return sys_read(regs->ebx, (char *)regs->ecx, regs->edx);
    case __NR_fork:    return sys_fork(regs);
    case __NR_wait4:   return sys_wait4(regs->ebx, (int *)regs->ecx);
    case __NR_execve:  return sys_execve((const char *)regs->ebx, regs);
    default:
        printk("[day19] unknown syscall %d\n",
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

/* ---- day19: 阻塞行读(来自 day08 键盘环形队列) ---- */
char tty_q_get(void);

static int sys_read(int fd, char *buf, int len)
{
    if (fd != 0 || len <= 0)
        return -1;
    int n = 0;
    while (n < len) {
        char c = tty_q_get();
        buf[n++] = c;
        if (c == '\n')
            break;
    }
    return n;
}

/* ---- day19: ring3 fork —— 复制含 syscall 现场的内核栈 ---- */
extern char fork_child_resume2[];
int do_fork(uint32_t caller_esp, const uint32_t *frame, int ndwords);

static int sys_fork(struct pt_regs *regs)
{
    uint32_t esp_now;
    __asm__ volatile("movl %%esp, %0" : "=r"(esp_now));
    uint32_t frame[2] = {
        (uint32_t)(uintptr_t)fork_child_resume2,
        (uint32_t)(uintptr_t)regs
    };
    return do_fork(esp_now, frame, 2);
}

/* ---- day19: execve —— 改写当前系统调用现场, iret 直达新程序 ---- */
#define EXEC_ADDR  0x80000
#define EXEC_STACK 0xB0000


int ramfs_size(const char *name);
int ramfs_read(const char *name, char *buf, uint32_t len);

static int sys_execve(const char *name, struct pt_regs *regs)
{
    int size = ramfs_size(name);
    if (size < 0 || size > 32768)
        return -1;
    memset((void *)EXEC_ADDR, 0, 32768);
    ramfs_read(name, (char *)EXEC_ADDR, size);
    regs->eip    = EXEC_ADDR;
    regs->oesp   = EXEC_STACK;
    regs->cs     = 0x1B;
    regs->eflags = 0x202;
    return 0;
}

/* ---- day19: waitpid(课程简化: 收割任意僵尸) ---- */
static int sys_wait4(int pid, int *status)
{
    (void)pid;
    for (;;) {
        for (int n = 1; n < NR_TASKS; n++) {
            if (task[n].state == TASK_ZOMBIE) {
                if (status)
                    *status = task[n].exit_code;
                int r = task[n].pid;
                task[n].state = TASK_DEAD;
                return r;
            }
        }
        __asm__ volatile("hlt");
    }
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
