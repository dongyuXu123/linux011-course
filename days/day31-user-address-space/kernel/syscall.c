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
extern char task_entry_stub[];
static int sys_execve(const char *name, struct pt_regs *regs);
static int sys_wait4(int pid, int *status);

/* 0.11 纪律: 进内核先把用户数据拷进内核缓冲 —— 时钟可能随时抢占
   并切换 CR3, 抢占后再摸用户指针读到的是别的任务的空间 */
static char kbuf[512];

/* 用户指针 = 任务空间内偏移; 内核侧线性 = 任务基址 + 偏移
   (0.11 的 get_fs_byte/set_fs_byte 干的就是这件事) */
#define UPTR(p) ((void *)(TASK_LIN_BASE((int)(current - task)) + \
                 (uint32_t)(uintptr_t)(p)))

static int sys_write(int fd, const char *buf, int len)
{
    if (fd != 1 || len <= 0 || len > 512)
        return -1;
    __asm__ volatile("cli");
    for (int i = 0; i < len; i++)
        kbuf[i] = ((char *)UPTR(buf))[i];
    __asm__ volatile("sti");
    printk("[w] cur=%c fd=%d len=%d\n", current->name, fd, len);
    for (int i = 0; i < len; i++)
        con_putc(kbuf[i]);
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
    if (fd != 0 || len <= 0 || len > 512)
        return -1;
    int n = 0;
    while (n < len) {
        char c = tty_q_get();
        kbuf[n++] = c;
        if (c == '\n')
            break;
    }
    __asm__ volatile("cli");       /* 回拷用户区: 不容抢占 */
    for (int i = 0; i < n; i++)
        ((char *)UPTR(buf))[i] = kbuf[i];
    __asm__ volatile("sti");
    return n;
}

/* ---- day19/31: ring3 fork —— 复制内核栈现场 + 0.11 copy_mem ---- */
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
    int pid = do_fork(esp_now, frame, 2);
    if (pid < 0)
        return -1;
    printk("[fk] parent user eip=%x esp=%x\n", regs->eip, regs->oesp);

    /* 0.11 copy_mem: 子任务独立页目录 + LDT(子基址), 用户页逐页复制 */
    int n = pid - 1;                        /* 槽位号(课程里 pid=slot+1) */
    (void)n;
    struct task_struct *child = 0;
    for (int i = 0; i < NR_TASKS; i++)
        if (task[i].pid == pid) { child = &task[i]; break; }
    if (!child)
        return -1;
    task_setup_mm((int)(child - task));     /* 新 cr3/LDT(子 64MB 基址) */
    printk("[fk2] slot=%d state=%d cr3=%x\n", (int)(child - task), child->state, child->cr3);

    /* 父用户区(pde_base 起 16 项)逐页复制到子任务同偏移 */

    for (uint32_t off = 0; off < 0x100000; off += 4096) {
        uint32_t pde = current->mm_pde_base + (off >> 22);
        uint32_t *pd = (uint32_t *)current->cr3;
        if (!(pd[pde] & 1))
            continue;
        uint32_t pte = (off >> 12) & 0x3FF;
        uint32_t *t = (uint32_t *)(pd[pde] & ~0xFFF);
        if (!(t[pte] & 1))
            continue;
        uint32_t phys = t[pte] & ~0xFFF;
        uint32_t nphys = get_free_page();
        if (!nphys)
            break;
        memcpy((void *)nphys, (void *)phys, 4096);      /* 0.11: 物理复制 */
        mm_map_page(child->cr3, TASK_LIN_BASE((int)(child - task)) + off, nphys);
    }
    return pid;
}

/* ---- day31: execve —— 0.11 地址模型: 程序装入本任务线性空间偏移 0,
   LDT 段基址(任务 64MB)由硬件加成; 栈顶在空间内 0xF0000 ---- */


int ramfs_size(const char *name);
int ramfs_read(const char *name, char *buf, uint32_t len);

/* day31: 0.11 地址模型 —— 程序装入本任务线性空间偏移 0(物理页从池取,
   挂进本任务页目录), LDT 段基址=任务 64MB 由硬件加成; iret 直达 */
static int sys_execve(const char *name, struct pt_regs *regs)
{
    const char *kname = (const char *)UPTR(name);
    int size = ramfs_size(kname);

    if (size < 0 || size > 32768)
        return -1;
    uint32_t base = TASK_LIN_BASE((int)(current - task));
    for (uint32_t off = 0; off < (uint32_t)size + 4095; off += 4096) {
        uint32_t phys = get_free_page();
        if (!phys)
            return -1;
        memset((void *)phys, 0, 4096);
        mm_map_page(current->cr3, base + off, phys);
    }
    ramfs_read(kname, (char *)(base + 0), size);  /* 经任务线性直写 */

    /* 用户栈: 0xE0000~0x100000 逐页分配挂映射 */
    for (uint32_t off = 0xE0000; off < 0x100000; off += 4096) {
        uint32_t phys = get_free_page();
        if (!phys)
            return -1;
        memset((void *)phys, 0, 4096);
        mm_map_page(current->cr3, base + off, phys);
    }
    regs->eip    = 0;                  /* 段内偏移 0, LDT 基址加成 */
    regs->esp    = USER_ESP;           /* 用户栈顶(空间内偏移) */
    regs->ss     = USER_DS;
    regs->cs     = USER_CS;
    regs->eflags = 0x202;
    printk("[x2] set: eip=%x esp=%x cs=%x ss=%x\n",
           regs->eip, regs->esp, regs->cs, regs->ss);
    return 0;
}

/* ---- day19: waitpid(课程简化: 收割任意僵尸) ---- */
static int sys_wait4(int pid, int *status)
{
    (void)pid;
    for (;;) {
        for (int n = 1; n < NR_TASKS; n++) {
            if (task[n].state == TASK_ZOMBIE) {
                if (status) {
                    __asm__ volatile("cli");
                    *(int *)UPTR(status) = task[n].exit_code;
                    __asm__ volatile("sti");
                }
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
