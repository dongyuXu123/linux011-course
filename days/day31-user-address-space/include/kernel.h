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
void task_setup_mm(int slot);                                  /* 建 cr3+LDT(0.11 copy_mem 前置) */
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
