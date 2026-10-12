/* =====================================================================
 * day14 - kernel/main.c：内核 shell —— 行编辑 + 内置命令
 * (0.11 没有内置 shell; 这是对它 /bin/sh 交互形态的课程化预演)
 * ===================================================================== */
#include <kernel.h>
#include <string.h>

void tty_init(void);
int  fb_init(void);
void con_serial_only_set(void);
void splash_show(void);
int  win_create(int x, int y, int w, int h, uint32_t rgb, const char *title);
void term_attach(int id);
void term_boot_banner(void);
int  term_dirty_flag(void);
void term_blink(void);
void gui_set_focus(int id);
void gui_refresh(int cx, int cy);
int  gui_pointer_step(int cx, int cy);
void mouse_init(void);
void mouse_task_update(void);
extern int cursor_x, cursor_y;
void con_clear(void);
void syscall_init(void);

extern volatile uint32_t jiffies;
extern struct task_struct task[NR_TASKS];

static inline void sti(void)  { __asm__ volatile("sti"); }
static inline void outb(uint16_t p, uint8_t v) { __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p)); }

void selftest_run(void);
void mm_load_cr3(uint32_t phys);
void mm_load_ldt(uint16_t sel);
int ata_init(void);
void blk_init(void);
int minix_mount(uint32_t dev);
void buffer_init(void);
int fs_read(const char *name, uint32_t off, char *buf, uint32_t len);
int fs_size(const char *name);
void kernel_task_create(int n, void (*fn)(void), char name);
extern const uint32_t saved_mbi, saved_magic;

/* multiboot_info 布局(只到 mods): GRUB 手册 "Boot information format" */
struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower, mem_upper;
    uint32_t boot_device, cmdline;
    uint32_t mods_count, mods_addr;
};

struct multiboot_mod_list {
    uint32_t mod_start, mod_end;
    uint32_t string, reserved;
};

/* day34: 挂载 Minix v1 根文件系统(day18 的 initrd 解析退役;
   init/hello/welcome/motd 都住在真文件系统里) */
static void minix_root_mount(void)
{
    if (minix_mount(0) < 0)
        panic("minix root mount failed");
}

/* 把 exec 进来的用户程序送上 ring3(与 day13 同款 iret 帧法) */
static void launch_user_80000(void)
{
    __asm__ volatile(
        "movl $0x23, %eax\n"
        "movl %eax, %ds\n"
        "movl %eax, %es\n"
        "movl %eax, %fs\n"
        "movl %eax, %gs\n"
        "pushl $0x23\n"
        "pushl $0xB0000\n"      /* 用户栈: 0xB0000 之上向下 */
        "pushl $0x202\n"
        "pushl $0x1B\n"
        "pushl $0x80000\n"
        "iret");
    for (;;) ;
}

static void launch_init(void)
{
    printk("[li] init task entered\n");
    /* 0.11 地址模型: cs/ds = 本任务 LDT 段(基址=64MB*槽位),
       eip/esp = 空间内偏移; 程序已由 main 装入 */
    __asm__ volatile(
        "movl $0x0F, %eax\n"
        "movl %eax, %ds\n"
        "movl %eax, %es\n"
        "movl %eax, %fs\n"
        "movl %eax, %gs\n"
        "pushl $0x0F\n"
        "pushl $0xF0000\n"
        "pushl $0x202\n"
        "pushl $0x07\n"
        "pushl $0x0\n"
        "iret");
    for (;;) ;
}

void fs_ls(void);

static void cmd_ps(void)
{
    static const char *st_name[] = { "run", "zomb", "dead" };
    printk("pid  state  name\n");
    for (int n = 0; n < NR_TASKS; n++)
        printk("%-4d %-6s %c\n", task[n].pid, st_name[task[n].state], task[n].name);
}

static void cmd_help(void)
{
    printk("builtins: help ps uptime clear echo ls cat write\n");
}

static void cmd_uptime(void)
{
    printk("uptime: %d.%02d s (jiffies %d)\n",
           jiffies / 100, jiffies % 100, jiffies);
}

/* cat <file>: 经 read API 把文件内容搬进内核缓冲再输出 */
static char catbuf[2048];

static void cmd_cat(const char *name)
{
    int n = fs_read(name, 0, catbuf, sizeof(catbuf) - 1);
    if (n < 0) {
        printk("cat: %s: no such file\n", name);
        return;
    }
    catbuf[n] = 0;
    printk("%s", catbuf);
    if (n && catbuf[n - 1] != '\n')
        printk("\n");
}

/* write <file> <text>: day34 只读挂载, 写路径 day35(0.11 sys.c) */
static void cmd_write(char *args)
{
    (void)args;
    printk("fs: read-only mount (write lands in day35)\n");
}

static void do_cmd(char *line)
{
    if (!strcmp(line, "help"))          { cmd_help(); return; }
    if (!strcmp(line, "ps"))            { cmd_ps(); return; }
    if (!strcmp(line, "uptime"))        { cmd_uptime(); return; }
    if (!strcmp(line, "clear"))         { con_clear(); return; }
    if (!strcmp(line, "ls"))            { fs_ls(); return; }
    if (!strncmp(line, "cat ", 4))      { cmd_cat(line + 4); return; }
    if (!strncmp(line, "write ", 6))    { cmd_write(line + 6); return; }
    const char *p = line;
    if (!strncmp(p, "echo ", 5))        { printk("%s\n", p + 5); return; }
    printk("unknown command: %s (try 'help')\n", line);
}

void kmain(void)
{
    con_init();
    {
        extern char _kernel_end[];
        struct multiboot_info { uint32_t flags, mem_lower, mem_upper; } *mbi =
            (void *)saved_mbi;
        mem_init(((uint32_t)(uintptr_t)_kernel_end + 4095) & ~4095, mbi->mem_upper);
    }
    fb_init();         /* day37: 图形模式先上(所有任务页目录继承 LFB) */
    con_serial_only_set();
    trap_init();
    irq_init();
    timer_init();
    sched_init();
    syscall_init();
    tty_init();
    buffer_init();     /* day34: buffer cache 先于文件系统 */
    blk_init();        /* day33: 块层先于一切块 I/O 就位 */
    ata_init();
    sti();
    minix_root_mount();   /* day34: 真文件系统上根 */

    /* 把 /sbin/init 送上 ring3(0.11 init/main.c 里 move_to_user_mode
       之后 exec /bin/sh 的课程版) */
    selftest_run();

    term_boot_banner();
    mouse_init();
    printk("[day37] ps/2 mouse ready\n");

    {
        /* 装入 /sbin/init 到 task1(槽1) 64MB 空间偏移 0。
           内核当前 cr3 是引导目录(无 task1 映射), 所以先经内核暂存区
           读入, 再逐页: 分配物理页 → 挂进 task1 页目录 → 物理直写 */
        static char stage[32768];
        int size = fs_size("init");
        if (size < 0 || size > 32768)
            panic("init image missing");
        fs_read("init", 0, stage, size);
        kernel_task_create(1, launch_init, 'I');
        struct task_struct *t1 = &task[1];
        for (uint32_t off = 0; off < (uint32_t)size + 4095; off += 4096) {
            uint32_t phys = get_free_page();
            if (!phys)
                panic("no page for init");
            memset((void *)phys, 0, 4096);
            memcpy((void *)phys, stage + off,
                   (size - off > 4096) ? 4096 : (size - off));
            mm_map_page(t1->cr3, TASK_LIN_BASE(1) + off, phys);
        }
        /* 用户栈: 0xF0000 往下 64KB(0xE0000~0x100000) */
        for (uint32_t off = 0xE0000; off < 0x100000; off += 4096) {
            uint32_t phys = get_free_page();
            memset((void *)phys, 0, 4096);
            mm_map_page(t1->cr3, TASK_LIN_BASE(1) + off, phys);
        }
    }
    /* ---- day37: 桌面搭建(task0 = 合成器) ---- */
    splash_show();
    printk("[day37] layer system online\n");
    int tw = win_create(20, 40, 280, 140, 0x00207030, "terminal");
    term_attach(tw);
    gui_set_focus(tw);
    gui_refresh(cursor_x, cursor_y);
    printk("[day37] first refresh done\n");
    printk("[day39] launching /sbin/init\n");
    for (;;) {                     /* task0: 合成器主循环 */
        int changed = gui_pointer_step(cursor_x, cursor_y);
        mouse_task_update();
        if (changed || term_dirty_flag() || gui_dirty_take())
            gui_refresh(cursor_x, cursor_y);
        term_blink();
        __asm__ volatile("hlt");
    }
}
