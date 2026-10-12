/* =====================================================================
 * kernel.h —— 课程内核公共声明(随课程逐课生长)
 * 对照: source/include/linux/sched.h 等 0.11 头文件
 * ===================================================================== */
#ifndef _KERNEL_H
#define _KERNEL_H

#include <stdint.h>

/* ---- 中断现场(压栈内存序: 段寄存器最后压=地址最低) ---- */
struct pt_regs {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, oesp;
    uint32_t ebx, edx, ecx, eax;
    uint32_t vector, err;
    uint32_t eip, cs, eflags;
    uint32_t esp, ss;       /* ring3 → ring0 时 CPU 压入(偏移 68/72) */
};

/* ---- console.c ---- */
void con_init(void);
void con_putc(char c);
void con_puts(const char *s);
void con_puthex(uint32_t v);
void con_clear(void);

/* ---- traps.c ---- */
void trap_init(void);
void trap_install_gate(int vec, uint32_t handler);
void trap_dispatch(struct pt_regs *regs);

/* ---- irq.c ---- */
typedef void (*irq_handler_t)(void *regs);
void irq_init(void);
void register_irq(int irq, irq_handler_t fn);

/* ---- printk.c ---- */
int printk(const char *fmt, ...);
void panic(const char *fmt, ...);

/* ---- mm.c ---- */
void mem_init(uint32_t start, uint32_t upper_kb);
uint32_t get_free_page(void);
void free_page(uint32_t addr);
uint32_t mem_free_pages(void);
uint32_t mm_pgdir_new(void);                                   /* 复制内核页目录 */
void mm_map_page(uint32_t pgdir_phys, uint32_t laddr, uint32_t phys);
void mm_load_cr3(uint32_t phys);
void mm_load_ldt(uint16_t sel);
int  do_no_page(uint32_t addr, uint32_t err);                 /* day32 按需调页 */
void task_setup_mm(int slot);                                  /* 建 cr3+LDT(0.11 copy_mem 前置) */

/* ---- buffer.c(day34): 缓冲区缓存 —— 0.11 fs/buffer.c ---- */
struct buffer_head {
    char        data[1024];
    uint32_t    dev;            /* 设备号(课程: 0=IDE 盘) */
    uint32_t    block;          /* 1KB 块号 */
    volatile int count;         /* 引用计数 */
    volatile int state;         /* 0空闲 1有效 2脏 */
    struct buffer_head *hnext;  /* hash 链 */
    struct buffer_head *fnext;  /* 空闲链(可复用) */
};
void buffer_init(void);
struct buffer_head *bread(uint32_t dev, uint32_t block);
void bwrite(struct buffer_head *bh);
void brelse(struct buffer_head *bh);
uint32_t buffer_stats(uint32_t *miss);

/* ---- minix.c(day34): Minix v1 文件系统(读路径) ---- */
int  minix_mount(uint32_t dev);
int  fs_lookup(const char *name);
int  fs_size(const char *name);
int  fs_read(const char *name, uint32_t off, char *buf, uint32_t len);
void fs_ls(void);
/* day35: 写路径(0.11 fs/bitmap.c + inode.c 写半边) */
int  fs_creat(const char *name);            /* 返回 inode 号 */
int  fs_write_ino(int ino, uint32_t off, const char *buf, uint32_t len);
int  fs_read_ino(int ino, uint32_t off, char *buf, uint32_t len);

/* ---- blk.c(day33): request 队列 —— 0.11 blk_drv/ll_rw_blk.c ---- */
#define BLK_READ  0
#define BLK_WRITE 1
struct blk_request {
    int      cmd;          /* BLK_READ / BLK_WRITE */
    uint32_t sector;       /* LBA */
    int      nsect;
    char    *buf;          /* 物理地址须在恒等映射内 */
    volatile int state;    /* 0空闲 1排队 2在飞 3完成 4失败 */
};
void blk_init(void);
int  ll_rw_blk(int cmd, uint32_t sector, int nsect, char *buf);
void blk_end_request(int ok);          /* 驱动中断上下文里调用 */
int  ata_start_req(struct blk_request *r);   /* ata.c: 程序化启动传输 */
extern uint32_t page_dir;                                      /* head.S 引导页目录 */
extern const uint32_t saved_mbi, saved_magic;

/* ---- sched.c: 任务模型(day11 起逐课生长) ---- */
#define NR_TASKS   4
#define TASK_LIN_BASE(n)  ((uint32_t)(n) * 0x4000000)  /* 0.11: 每任务 64MB 线性空间 */
#define TASK_LDT_SEL(n)   ((uint16_t)(0x30 + (n) * 8)) /* GDT 6 号槽起放 LDT */
#define USER_CS  0x07                 /* LDT[0] code |TI|RPL3 */
#define USER_DS  0x0F                 /* LDT[1] data |TI|RPL3 */
#define USER_ESP 0xF0000              /* 用户栈顶(线性空间内偏移) */

struct task_struct {
    volatile long state;        /* 0 runnable, 1 zombie, 2 dead */
    long counter;
    long priority;
    int  pid;
    int  exit_code;
    char name;
    uint32_t kesp;
    uint32_t kstack_top;    /* 本任务内核栈顶(do_fork 复制基准) */
    uint32_t cr3;           /* 页目录物理地址(每任务独立, 0.11 copy_mem) */
    uint32_t ldt_sel;       /* LDT 描述符选择符 */
    uint32_t ldt[4];        /* LDT: [0]代码 [1]数据 (base=本任务 64MB) */
    uint32_t mm_pde_base;   /* 本任务用户区起始 PDE 序号 */
    char exe_name[16];      /* 按需调页: 缺页时从该文件读程序页(0.11 inode) */
    uint32_t ex_size;       /* 映像文件大小; 超过它的用户页由零页填充 */
    struct file {           /* day35: 打开文件表(0.11 filp[20] 课程版) */
        int      ino;       /* 0=未用 */
        uint32_t off;
    } fds[8];
};
enum { TASK_RUNNABLE = 0, TASK_ZOMBIE = 1, TASK_DEAD = 2 };
extern struct task_struct *current;
extern volatile uint32_t jiffies;
extern struct task_struct task[NR_TASKS];

/* ---- sched.c (day12) ---- */
void sched_init(void);
void kernel_task_create(int n, void (*fn)(void), char name);
void schedule(void);
void do_timer(void);
int  fork(void);
void exit(int code) __attribute__((noreturn));
int  wait(int *status);

/* ---- syscall.c ---- */
struct idt_entry {
    uint16_t off_lo;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  type_attr;
    uint16_t off_hi;
} __attribute__((packed));
void syscall_init(void);

/* ---- timer.c ---- */
void sched_init(void);
void kernel_task_create(int n, void (*fn)(void), char name);
void schedule(void);
void do_timer(void);

/* ---- syscall.c ---- */
struct idt_entry {
    uint16_t off_lo;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  type_attr;
    uint16_t off_hi;
} __attribute__((packed));
void syscall_init(void);

/* ---- timer.c ---- */
void timer_init(void);
extern volatile uint32_t jiffies;

#endif /* _KERNEL_H */
int strncmp(const char *a, const char *b, unsigned n);
char *strchr(const char *s, int c);
