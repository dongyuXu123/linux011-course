/* =====================================================================
 * day14 - kernel/main.c：内核 shell —— 行编辑 + 内置命令
 * (0.11 没有内置 shell; 这是对它 /bin/sh 交互形态的课程化预演)
 * ===================================================================== */
#include <kernel.h>
#include <string.h>

void tty_init(void);
char tty_getc(void);
void con_clear(void);
void syscall_init(void);

extern volatile uint32_t jiffies;
extern struct task_struct task[NR_TASKS];

static inline void sti(void)  { __asm__ volatile("sti"); }
static inline void outb(uint16_t p, uint8_t v) { __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p)); }

void ramfs_init(void);
int ramfs_read(const char *name, char *buf, uint32_t len);
int ramfs_create(const char *name);
int ramfs_write(const char *name, const char *buf, uint32_t len);
int wait(int *status);
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

/* 解析 GRUB module 加载的 initrd(romfs-lite: [nlen|name|dlen|data]*) */
static void initrd_load(void)
{
    struct multiboot_info *mbi = (struct multiboot_info *)saved_mbi;
    if (!(mbi->flags & (1 << 3)) || mbi->mods_count == 0) {
        printk("[day18] no initrd module from GRUB!\n");
        return;
    }
    struct multiboot_mod_list *mod = (struct multiboot_mod_list *)mbi->mods_addr;
    uint32_t size = mod->mod_end - mod->mod_start;
    printk("[day18] GRUB loaded %d module(s), initrd at %x (%d bytes)\n",
           mbi->mods_count, mod->mod_start, size);

    uint8_t *p = (uint8_t *)mod->mod_start;
    uint8_t *end = (uint8_t *)mod->mod_end;
    int files = 0;
    while (p + 4 <= end) {
        uint32_t nlen = *(uint32_t *)p; p += 4;
        if (nlen == 0 || nlen > 32 || p + nlen + 4 > end)
            break;
        char name[33];
        memcpy(name, p, nlen); name[nlen] = 0; p += nlen;
        uint32_t dlen = *(uint32_t *)p; p += 4;
        if (p + dlen > end)
            break;
        ramfs_create(name);
        ramfs_write(name, (char *)p, dlen);
        printk("[day18] initrd: added %s (%d bytes)\n", name, dlen);
        p += dlen;
        files++;
    }
    printk("[day18] initrd: %d file(s) into ramfs\n", files);
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

/* run <prog>: 从 ramfs 读到 0x80000, 开任务上 ring3, 等它退出 */
static void cmd_run(const char *name)
{
    if (ramfs_read(name, (char *)0x80000, 8192) < 0) {
        printk("run: %s: no such file\n", name);
        return;
    }
    printk("run: exec %s at 0x80000\n", name);
    kernel_task_create(1, launch_user_80000, 'H');
    int st = 0;
    int pid = wait(&st);
    printk("run: %s (pid %d) exited with %d\n", name, pid, st);
}
void ramfs_ls(void);
int ramfs_create(const char *name);
int ramfs_write(const char *name, const char *buf, uint32_t len);
int ramfs_read(const char *name, char *buf, uint32_t len);
int ramfs_size(const char *name);

static void cmd_ps(void)
{
    static const char *st_name[] = { "run", "zomb", "dead" };
    printk("pid  state  name\n");
    for (int n = 0; n < NR_TASKS; n++)
        printk("%-4d %-6s %c\n", task[n].pid, st_name[task[n].state], task[n].name);
}

static void cmd_help(void)
{
    printk("builtins: help ps uptime clear echo ls cat write run\n");
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
    int n = ramfs_read(name, catbuf, sizeof(catbuf) - 1);
    if (n < 0) {
        printk("cat: %s: no such file\n", name);
        return;
    }
    catbuf[n] = 0;
    printk("%s", catbuf);
    if (n && catbuf[n - 1] != '\n')
        printk("\n");
}

/* write <file> <text...>: 创建(必要时)并整体覆写 */
static void cmd_write(char *args)
{
    char *sp = strchr(args, ' ');
    if (!sp) {
        printk("usage: write <file> <text>\n");
        return;
    }
    *sp = 0;
    if (ramfs_create(args) < 0 && ramfs_size(args) < 0) {
        printk("write: cannot create %s\n", args);
        return;
    }
    int n = ramfs_write(args, sp + 1, strlen(sp + 1));
    printk("wrote %d bytes to %s\n", n, args);
}

static void do_cmd(char *line)
{
    if (!strcmp(line, "help"))          { cmd_help(); return; }
    if (!strcmp(line, "ps"))            { cmd_ps(); return; }
    if (!strcmp(line, "uptime"))        { cmd_uptime(); return; }
    if (!strcmp(line, "clear"))         { con_clear(); return; }
    if (!strcmp(line, "ls"))            { ramfs_ls(); return; }
    if (!strncmp(line, "cat ", 4))      { cmd_cat(line + 4); return; }
    if (!strncmp(line, "write ", 6))    { cmd_write(line + 6); return; }
    if (!strncmp(line, "run ", 4))      { cmd_run(line + 4); return; }
    const char *p = line;
    if (!strncmp(p, "echo ", 5))        { printk("%s\n", p + 5); return; }
    printk("unknown command: %s (try 'help')\n", line);
}

void kmain(void)
{
    con_init();
    trap_init();
    irq_init();
    timer_init();
    sched_init();
    syscall_init();
    tty_init();
    ramfs_init();
    /* 预置两个文件, ls 立即有内容可看 */
    ramfs_create("welcome.txt");
    ramfs_write("welcome.txt", "welcome to linux011-course day15 ramfs\n", 39);
    ramfs_create("motd");
    ramfs_write("motd", "day15: ramfs is alive\n", 22);
    initrd_load();     /* GRUB module → ramfs */
    sti();

    printk("[day18] linux011 mini shell -- type 'help'\n");

    char line[64];
    for (;;) {
        printk("linux011> ");
        int n = 0;
        for (;;) {
            char c = tty_getc();
            if (c == '\n') { line[n] = 0; con_putc('\n'); break; }
            if (c == '\b') { if (n > 0) n--; continue; }
            if (c == '\r') continue;
            if (n < 63) line[n++] = c;
        }
        if (line[0])
            do_cmd(line);
    }
}
