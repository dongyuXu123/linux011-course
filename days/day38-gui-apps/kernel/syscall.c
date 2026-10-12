/* =====================================================================
 * day31 - kernel/syscall.c：系统调用(0.11 ABI 子集)
 * 调用号沿用 0.11: exit=1 fork=2 read=3 write=4 execve=11 getpid=20
 * 用户指针均为任务空间内偏移, 经 UPTR 翻译为内核线性地址
 * (0.11 get_fs_byte/set_fs_byte 的对应物); 访问用户数据前后
 * cli/sti, 防止时钟抢占切换 CR3 后摸错地址空间。
 * ===================================================================== */
#include <kernel.h>
#include <string.h>

int fs_size(const char *name);
int fs_creat(const char *name);
int fs_write_ino(int ino, uint32_t off, const char *buf, uint32_t len);

/* 阻塞读一行(day36 行规程) */
int tty_ldisc_read(char *out, int max);

#define __NR_exit     1
#define __NR_fork     2
#define __NR_read     3
#define __NR_write    4
#define __NR_open     5
#define __NR_close    6
#define __NR_creat    8
#define __NR_lseek    19
#define __NR_time     13        /* 0.11 同号: 秒(jiffies/100) */
#define __NR_win_create 41      /* 0.11 未用区: 课程图形窗口 API */
#define __NR_win_write  42
#define __NR_execve   11
#define __NR_getpid   20
#define __NR_wait4    72

static char kbuf[512];

/* 用户指针 → 内核线性地址 */
#define UPTR(p) ((void *)(TASK_LIN_BASE((int)(current - task)) + \
                 (uint32_t)(uintptr_t)(p)))

static int sys_write(int fd, const char *buf, int len)
{
    if (len <= 0 || len > 512)
        return -1;
    __asm__ volatile("cli");               /* 用户数据先拷进内核缓冲 */
    for (int i = 0; i < len; i++)
        kbuf[i] = ((char *)UPTR(buf))[i];
    __asm__ volatile("sti");
    if (fd == 1) {                         /* 控制台 */
        for (int i = 0; i < len; i++)
            con_putc(kbuf[i]);
        return len;
    }
    if (fd >= 2 && fd < 8 && current->fds[fd].ino)   /* 文件 */
        return fs_write_ino(current->fds[fd].ino,
                            current->fds[fd].off, kbuf, len);
    return -1;
}

/* ---- day35: open/creat/close/lseek(0.11 sys_open 家族课程版) ----
   fd0=stdin(tty) fd1=stdout(console) 固定; 文件 fd 从 2 起 */
static int sys_open_creat(const char *name, int creat_new)
{
    __asm__ volatile("cli");               /* 路径名拷贝不容抢占 */
    {
        const char *u = (const char *)UPTR(name);
        int i = 0;
        while (i < 511 && u[i]) { kbuf[i] = u[i]; i++; }
        kbuf[i] = 0;
    }
    __asm__ volatile("sti");
    int ino = fs_lookup(kbuf);
    if (!ino && creat_new)
        ino = fs_creat(kbuf);
    if (!ino)
        return -1;
    for (int fd = 2; fd < 8; fd++)
        if (!current->fds[fd].ino) {
            current->fds[fd].ino = ino;
            current->fds[fd].off = 0;
            return fd;
        }
    return -1;
}

static int sys_open(const char *name, int flags)
{
    (void)flags;
    return sys_open_creat(name, 0);
}

static int sys_creat(const char *name, int mode)
{
    (void)mode;
    return sys_open_creat(name, 1);
}

static int sys_close(int fd)
{
    if (fd >= 2 && fd < 8 && current->fds[fd].ino) {
        current->fds[fd].ino = 0;
        return 0;
    }
    return -1;
}

static int sys_lseek(int fd, int off, int whence);

/* ---- day37: 图形窗口 API(0.11 无图形; 30dayMakeOS day20 API 思想
   落成系统调用, 应用经 ABI 建窗/写窗) ---- */
static volatile int gui_dirty = 0;   /* 有新内容: 主循环尽快刷新 */
int gui_dirty_take(void)
{
    if (gui_dirty) {
        gui_dirty = 0;
        return 1;
    }
    return 0;
}

static int sys_win_create(int x, int y, int w, int h, const char *name)
{
    __asm__ volatile("cli");
    {
        const char *u = (const char *)UPTR(name);
        int i = 0;
        while (i < 511 && u[i]) { kbuf[i] = u[i]; i++; }
        kbuf[i] = 0;
    }
    __asm__ volatile("sti");
    int id = win_create(x, y, w, h, 0x004060A0, kbuf);
    gui_dirty = 1;
    printk("[day37] win %d created at %dx%d %dx%d '%s'\n",
           id, x, y, w, h, kbuf);
    return id;
}

static int sys_win_write(int id, const char *buf, int len)
{
    if (len <= 0 || len > 512)
        return -1;
    __asm__ volatile("cli");
    for (int i = 0; i < len; i++)
        kbuf[i] = ((char *)UPTR(buf))[i];
    __asm__ volatile("sti");
    extern int win_rect_xy(int id, int *x, int *y);
    int x, y;
    if (!win_rect_xy(id, &x, &y))
        return -1;
    kbuf[len] = 0;
    gfx_putstr(kbuf, x + 8, y + 24, 0x00E8E8E8, 0x004060A0);
    /* 落像素证明: 写一个标记色并立即读回 */
    fb_pixel(x + 4, y + 4, 0x00FF00FF);
    fb_flush_current();
    uint32_t px = fb_getpixel(x + 4, y + 4);
    gui_dirty = 1;
    printk("[day37] win %d write %d chars, marker px=%x\n", id, len, px);
    return len;
}

static int sys_lseek(int fd, int off, int whence)
{
    if (fd < 2 || fd >= 8 || !current->fds[fd].ino)
        return -1;
    uint32_t *pos = &current->fds[fd].off;
    if (whence == 0)
        *pos = (uint32_t)off;              /* SEEK_SET */
    else if (whence == 1)
        *pos += off;                       /* SEEK_CUR */
    else
        return -1;                         /* SEEK_END 略 */
    return (int)*pos;
}

static int sys_getpid(void)
{
    return current->pid;
}


static int sys_read(int fd, char *buf, int len)
{
    if (len <= 0 || len > 512)
        return -1;
    int n;
    if (fd == 0) {                         /* stdin: 规程整行交付 */
        n = tty_ldisc_read(kbuf, len);
    } else if (fd >= 2 && fd < 8 && current->fds[fd].ino) {
        n = fs_read_ino(current->fds[fd].ino, current->fds[fd].off,
                        kbuf, len);
        if (n < 0)
            return -1;
    } else
        return -1;
    __asm__ volatile("cli");       /* 回拷用户区: 不容抢占 */
    for (int i = 0; i < n; i++)
        ((char *)UPTR(buf))[i] = kbuf[i];
    __asm__ volatile("sti");
    if (fd >= 2)
        current->fds[fd].off += n;
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

/* ---- execve: 登记映像 + 换 iret 现场(day32 按需调页版) ----
   0.11 do_execve 不预装程序页: 只在任务里登记可执行文件, 页面等
   #PF 由 do_no_page 从设备按需读入。课程对应: 记 exe_name/ex_size,
   清掉旧映像的用户页(栈保留), 再按 0.11 start_thread 改写 iret 帧;
   本系统调用"返回"时 CPU 直接落入新程序入口, 第一条取指就触发
   do_no_page 把第 0 页读进来。 */
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

    int size = fs_size(kbuf);
    if (size < 0 || size > 65536)
        return -1;

    /* 清旧映像: 用户区 [0, 0xE0000) 的页全部解除并归还
       (栈 0xE0000~0x100000 保留 —— 0.11 do_execve 亦保留栈段)。
       注意查表必须用完整线性地址: 用户页挂在 PDE slot*16 起的表里,
       直接拿偏移查会误入内核低位恒等映射的 0 号表 */
    {
        uint32_t *pd = (uint32_t *)current->cr3;
        for (uint32_t off = 0; off < 0xE0000; off += 4096) {
            uint32_t laddr = TASK_LIN_BASE(slot) + off;
            uint32_t pde = laddr >> 22, pte = (laddr >> 12) & 0x3FF;
            if (!(pd[pde] & 1))
                continue;
            uint32_t *t = (uint32_t *)(pd[pde] & ~0xFFF);
            if (t[pte] & 1) {
                free_page(t[pte] & ~0xFFF);
                t[pte] = 0;
            }
        }
        uint32_t cr3;                      /* 清映射后统一刷 TLB */
        __asm__ volatile("movl %%cr3, %0" : "=r"(cr3));
        __asm__ volatile("movl %0, %%cr3" : : "r"(cr3));
    }

    /* 登记可执行文件: 之后的一切页面供给由 do_no_page 负责 */
    for (int i = 0; i < 15 && kbuf[i]; i++)
        current->exe_name[i] = kbuf[i];
    current->exe_name[15] = 0;
    current->ex_size = (uint32_t)size;

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
    case __NR_open:    return sys_open((const char *)(uintptr_t)regs->ebx, regs->ecx);
    case __NR_creat:   return sys_creat((const char *)(uintptr_t)regs->ebx, regs->ecx);
    case __NR_close:   return sys_close(regs->ebx);
    case __NR_lseek:   return sys_lseek(regs->ebx, (int)regs->ecx, (int)regs->edx);
    case __NR_time:    return (int)(jiffies / 100);
    case __NR_win_create: return sys_win_create((int)regs->ebx, (int)regs->ecx,
                                                (int)regs->edx, (int)regs->esi,
                                                (const char *)(uintptr_t)regs->edi);
    case __NR_win_write:  return sys_win_write(regs->ebx,
                                               (const char *)(uintptr_t)regs->ecx,
                                               (int)regs->edx);
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
