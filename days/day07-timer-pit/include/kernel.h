/* =====================================================================
 * kernel.h —— 课程内核公共声明(随课程逐课生长)
 * 对照: source/include/linux/sched.h 等 0.11 头文件
 * ===================================================================== */
#ifndef _KERNEL_H
#define _KERNEL_H

#include <stdint.h>

/* ---- 中断现场(trap_entry.S/irq_entry.S 压栈布局) ---- */
struct pt_regs {
    uint32_t edi, esi, ebp, oesp;
    uint32_t ebx, edx, ecx, eax;
    uint32_t ds, es, fs, gs;
    uint32_t vector, err;
    uint32_t eip, cs, eflags;
};

/* ---- console.c ---- */
void con_init(void);
void con_putc(char c);
void con_puts(const char *s);
void con_puthex(uint32_t v);

/* ---- traps.c ---- */
void trap_init(void);
void trap_install_gate(int vec, uint32_t handler);
void trap_dispatch(struct pt_regs *regs);

/* ---- irq.c ---- */
typedef void (*irq_handler_t)(void *regs);
void irq_init(void);
void register_irq(int irq, irq_handler_t fn);

/* ---- timer.c ---- */
void timer_init(void);
extern volatile uint32_t jiffies;

#endif /* _KERNEL_H */
