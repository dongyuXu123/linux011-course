/* =====================================================================
 * day31 - kernel/syscall.c：系统调用(0.11 ABI 子集)
 * 调用号沿用 0.11: exit=1 fork=2 read=3 write=4 execve=11 getpid=20
 * 用户指针均为任务空间内偏移, 经 UPTR 翻译为内核线性地址
 * (0.11 get_fs_byte/set_fs_byte 的对应物); 访问用户数据前后
 * cli/sti, 防止时钟抢占切换 CR3 后摸错地址空间。
 * ===================================================================== */
#include <kernel.h>
#include <string.h>

int ramfs_read(const char *name, char *buf, uint32_t len);
int  ramfs_size(const char *name);

/* 阻塞取键(day08 键盘环形队列) */
char tty_q_get(void);

#define __NR_exit     1
#define __NR_fork     2
#define __NR_read     3
#define __NR_write    4
#define __NR_execve   11
#define __NR_getpid   20
#define __NR_wait4    72

static char kbuf[512];

/* 用户指针 → 内核线性地址 */
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
    for (int i = 0; i < len; i++)
        con_putc(kbuf[i]);
    return len;
}

static int sys_getpid(void)
{
    return current->pid;
}

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

/* ---- fork: 复制内核栈现场 + 0.11 copy_mem(用户页逐页复制) ---- */
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
    struct task_struct *child = 0;
    for (int i = 0; i < NR_TASKS; i++)
        if (task[i].pid == pid) { child = &task[i]; break; }
    if (!child)
        return -1;

    /* 0.11 copy_mem: 子任务独立页目录 + LDT(子 64MB 基址),
       父用户区逐页物理复制到子空间同偏移 */
    int cslot = (int)(child - task);
    uint32_t saved_child_kesp = child->kesp;
    uint32_t saved_child_state = child->state;
    task_setup_mm(cslot);
    child->kesp = saved_child_kesp;
    child->state = saved_child_state;
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
        memcpy((void *)nphys, (void *)phys, 4096);
        mm_map_page(child->cr3, TASK_LIN_BASE(cslot) + off, nphys);
    }
    return pid;
}

/* ---- execve: 整个替换任务映像(0.11 sys_execve + start_thread) ----
   程序装入本任务空间偏移 0; 然后改写系统调用现场(iret 帧), 本系统
   调用"返回"时 CPU 直接落入新程序入口 —— 0.11 start_thread 同法 */
static int sys_execve(const char *name, struct pt_regs *regs)
{
    int slot = (int)(current - task);
    __asm__ volatile("cli");               /* 拷贝路径名: 不容抢占切 CR3 */
    {
        const char *u = (const char *)UPTR(name);
        int i = 0;
        while (i < 511 && u[i]) { kbuf[i] = u[i]; i++; }
        kbuf[i] = 0;
    }
    __asm__ volatile("sti");

    int size = ramfs_size(kbuf);
    if (size < 0 || size > 32768)
        return -1;
    for (uint32_t off = 0; off < (uint32_t)size + 4095; off += 4096) {
        uint32_t phys = get_free_page();
        if (!phys)
            return -1;
        memset((void *)phys, 0, 4096);
        mm_map_page(current->cr3, TASK_LIN_BASE(slot) + off, phys);
    }
    ramfs_read(kbuf, (char *)TASK_LIN_BASE(slot), size);
    /* 用户栈: 0xF0000 往下 64KB(0xE0000~0x100000) */
    for (uint32_t soff = 0xE0000; soff < 0x100000; soff += 4096) {
        uint32_t phys = get_free_page();
        if (!phys)
            return -1;
        memset((void *)phys, 0, 4096);
        mm_map_page(current->cr3, TASK_LIN_BASE(slot) + soff, phys);
    }

    /* start_thread: 换段/换入口/换栈, iret 即新程序 */
    regs->cs = 0x07;   regs->eip = 0;
    regs->ss = 0x0F;   regs->esp = 0xF0000;
    regs->ds = regs->es = regs->fs = regs->gs = 0x0F;
    regs->eflags = 0x202;
    return 0;
}

/* ---- wait4: 收割任意僵尸(课程简化) ---- */
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

int syscall_dispatch(struct pt_regs *regs)
{
    switch (regs->eax) {
    /* 各 sys_* 内部自己做 UPTR 翻译, 这里传空间内偏移原值 */
    case __NR_write:   return sys_write(regs->ebx, (const char *)(uintptr_t)regs->ecx, regs->edx);
    case __NR_getpid:  return sys_getpid();
    case __NR_exit:    exit(regs->ebx);   /* noreturn */
    case __NR_read:    return sys_read(regs->ebx, (char *)(uintptr_t)regs->ecx, regs->edx);
    case __NR_fork:    return sys_fork(regs);
    case __NR_wait4:   return sys_wait4(regs->ebx, (int *)(uintptr_t)regs->ecx);
    case __NR_execve:  return sys_execve((const char *)(uintptr_t)regs->ebx, regs);
    default:
        printk("[day31] unknown syscall %d\n", regs->eax);
        return -1;
    }
}

/* ---- 系统 TSS: GDT 5 号槽描述符由这里回填(head.S 里只是占位) ---- */
static void tss_install(void)
{
    extern char gdt[], system_tss[];
    uint32_t base = (uint32_t)(uintptr_t)system_tss;
    uint32_t lim  = 103;
    uint8_t *d = (uint8_t *)gdt + 5 * 8;
    d[0] = lim & 0xFF;        d[1] = (lim >> 8) & 0xFF;
    d[2] = base & 0xFF;       d[3] = (base >> 8) & 0xFF;
    d[4] = (base >> 16) & 0xFF;
    d[5] = 0x89;
    d[6] = (lim >> 16) & 0x0F;
    d[7] = (base >> 24) & 0xFF;
    uint32_t *t = (uint32_t *)system_tss;
    t[0] = 0;
    t[1] = 0;                 /* esp0: 每次任务切换时更新(sched.c) */
    t[2] = 0x10;
    __asm__ volatile("ltr %%ax" : : "a"((uint16_t)0x28));
}

void syscall_init(void)
{
    tss_install();
    extern char system_call[];   /* kernel/syscall_entry.S */
    trap_install_gate(0x80, (uint32_t)(uintptr_t)system_call);
    extern struct idt_entry idt[256];
    idt[0x80].type_attr = 0xEF;  /* DPL3: ring3 可以 int 0x80 */
}
