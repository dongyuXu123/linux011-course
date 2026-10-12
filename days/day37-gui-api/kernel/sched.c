/* =====================================================================
 * day12 - kernel/sched.c：fork/exit/wait —— 任务生命周期
 * ---------------------------------------------------------------------
 * 0.11 fork() 的核心: 复制父进程的地址空间与内核栈, 父子从 fork
 * 返回点分叉, 子得到 0、父得到子 pid。我们的内核线程版: 复制父
 * 任务的内核栈(调用链原样带走), 子栈顶伪造恢复帧, 被调度时带着
 * fork()==0 回到父代码路径。
 *
 * 对照阅读: source/kernel/fork.c(copy_process)、source/kernel/exit.c
 * ===================================================================== */
#include <kernel.h>
#include <string.h>

struct task_struct task[NR_TASKS];
struct task_struct *current = &task[0];

extern void cpu_switch_to(uint32_t *prev_esp_out, uint32_t next_esp);
extern char task_entry_stub[];
extern char fork_child_resume[];

static char task_stack[NR_TASKS][4096] __attribute__((aligned(16)));

static int next_pid = 1;

void kernel_task_create(int n, void (*fn)(void), char name)
{
    uint32_t *sp = (uint32_t *)(task_stack[n] + sizeof(task_stack[n]));
    *--sp = (uint32_t)fn;
    *--sp = (uint32_t)(uintptr_t)task_entry_stub;

    task[n].state = TASK_RUNNABLE;
    task[n].priority = 10;
    task[n].counter = task[n].priority;
    task[n].pid = next_pid++;
    task[n].exit_code = 0;
    task[n].kesp = (uint32_t)sp;
    task[n].kstack_top = (uint32_t)(task_stack[n] + sizeof(task_stack[n]));
    task_setup_mm(n);                  /* day31: cr3 + LDT + 用户区 PDE 基址 */
    task[n].name = name;
}

/* ---- day31: C 侧 GDT(含每任务 LDT 槽) + 任务内存初始化 ---- */
struct desc_struct {
    uint16_t lim_lo, base_lo;
    uint8_t  base_mid, type;
    uint8_t  lim_hi, base_hi;
} __attribute__((packed));

struct desc_struct gdt[16];   /* 全局: syscall.c tss_install 回填 TSS 描述符 */
static struct { uint16_t limit; uint32_t base; } __attribute__((packed)) gdt_ptr;

/* LDT: [0]代码(DPL3) [1]数据(DPL3), base=任务 64MB 基址 */
static void set_ldt_desc(int slot, uint32_t base)
{
    uint32_t lim = 0x3FFF;                 /* 64MB, 4KB 粒度 */
    int gi = 6 + slot;                     /* GDT 6~9 号槽 = LDT0~3 */
    gdt[gi] = (struct desc_struct){
        .lim_lo = lim & 0xFFFF, .base_lo = base & 0xFFFF,
        .base_mid = (base >> 16) & 0xFF, .type = 0x82,
        .lim_hi = (lim >> 16) & 0x0F, .base_hi = (base >> 24) & 0xFF };
}

/* 每任务的 LDT 内容: 代码/数据段, base=64MB*slot, 4KB 粒度 32 位 */
struct desc_struct {
    uint16_t lim_lo, base_lo;
    uint8_t  base_mid, type;
    uint8_t  lim_hi, base_hi;
} __attribute__((packed));

static void ldt_set_desc(uint32_t *ldt, int idx, uint32_t base, uint8_t type)
{
    struct desc_struct d = {
        .lim_lo = 0xFFFF, .base_lo = base & 0xFFFF,
        .base_mid = (base >> 16) & 0xFF, .type = type,
        .lim_hi = 0xCF,                      /* G=1, D=1, limit 高 4 位=F */
        .base_hi = (base >> 24) & 0xFF };
    uint32_t *p = ldt + idx * 2;
    p[0] = *(uint32_t *)&d;
    p[1] = *(uint32_t *)((uint8_t *)&d + 4);
}

static void task_ldt_build(int slot)
{
    uint32_t base = TASK_LIN_BASE(slot);
    ldt_set_desc(task[slot].ldt, 0, base, 0xFA);   /* 代码 DPL3 */
    ldt_set_desc(task[slot].ldt, 1, base, 0xF2);   /* 数据 DPL3 */
}

void task_setup_mm(int slot)
{
    task[slot].cr3 = mm_pgdir_new();       /* 复制内核页目录(0.11 copy_mem 前置) */
    task_ldt_build(slot);
    task[slot].ldt_sel = TASK_LDT_SEL(slot);
    task[slot].mm_pde_base = slot * 16;    /* 用户区起始 PDE(64MB/4MB=16 项) */
}

void sched_init(void)
{
    for (int n = 1; n < NR_TASKS; n++)
        task[n].state = TASK_DEAD;     /* 空槽: 可被 fork 占用 */
    task[0].state = TASK_RUNNABLE;
    task[0].priority = 10;
    task[0].counter = 10;
    task[0].pid = 0;
    task[0].name = '0';
    extern char stack_top[];
    task[0].kstack_top = (uint32_t)(uintptr_t)stack_top;
    task[0].cr3 = (uint32_t)(uintptr_t)&page_dir;   /* 引导页目录 */
    task[0].mm_pde_base = 0;

    /* ---- C 侧 GDT: 内核段/用户段/TSS/每任务 LDT ---- */
    gdt[1] = (struct desc_struct){ .lim_lo = 0xFFFF, .type = 0x9A, .lim_hi = 0xCF };
    gdt[2] = (struct desc_struct){ .lim_lo = 0xFFFF, .type = 0x92, .lim_hi = 0xCF };
    gdt[3] = (struct desc_struct){ .lim_lo = 0xFFFF, .type = 0xFA, .lim_hi = 0xCF };
    gdt[4] = (struct desc_struct){ .lim_lo = 0xFFFF, .type = 0xF2, .lim_hi = 0xCF };
    for (int n = 0; n < NR_TASKS; n++) {
        set_ldt_desc(n, (uint32_t)(uintptr_t)&task[n].ldt); /* LDT 描述符基址=ldt 结构体 */
        task_ldt_build(n);
        task[n].ldt_sel = TASK_LDT_SEL(n);
        task[n].mm_pde_base = n * 16;
    }
    gdt_ptr.limit = sizeof(gdt) - 1;
    gdt_ptr.base  = (uint32_t)gdt;
    __asm__ volatile("lgdt %0" : : "m"(gdt_ptr));
    __asm__ volatile("ljmp $0x08, $1f\n1:");
    __asm__ volatile("movl $0x10, %eax\n movl %eax, %ds\n movl %eax, %es\n"
                     "movl %eax, %fs\n movl %eax, %gs\n movl %eax, %ss");
}

/* ---- fork: 复制当前任务的内核栈(调用方给出恢复帧) ---- */

int do_fork(uint32_t caller_esp, const uint32_t *frame, int ndwords)
{
    int n;
    for (n = 1; n < NR_TASKS; n++)
        if (task[n].state == TASK_DEAD)
            break;
    if (n >= NR_TASKS)
        return -1;

    /* 复制父栈在用区间 [caller_esp, 父栈顶) —— 含全部调用链/系统
       调用现场, 子进程经恢复帧从"同一位置"继续 */
    uint32_t used = current->kstack_top - caller_esp;
    char *cbase = task_stack[n] + sizeof(task_stack[n]);
    memcpy(cbase - used, (void *)caller_esp, used);

    /* 恢复帧里凡指向父栈区间[caller_esp, 栈顶)的指针, 平移到子栈
       副本上的对应位置 —— 否则子进程会跑回父进程的内核栈 */
    uint32_t delta = (uint32_t)(cbase - used) - caller_esp;
    uint32_t *f = (uint32_t *)(cbase - used - ndwords * 4);
    for (int i = 0; i < ndwords; i++) {
        uint32_t v = frame[i];
        if (v >= caller_esp && v < (uint32_t)caller_esp + used)
            v += delta;
        f[i] = v;
    }

    task[n].state = TASK_RUNNABLE;
    task[n].priority = current->priority;
    task[n].counter = current->priority;
    task[n].pid = next_pid++;
    task[n].exit_code = 0;
    task[n].kesp = (uint32_t)f;
    task[n].kstack_top = (uint32_t)cbase;
    task[n].name = current->name;
    return task[n].pid;
}

void exit(int code)
{
    printk("[day37] task pid %d exit(%d)\n", current->pid, code);
    current->exit_code = code;
    current->state = TASK_ZOMBIE;
    schedule();
    for (;;) ;
}

/* wait: 收割一个僵尸子任务(课程简化: 同名任务视为当前任务的子) */
int wait(int *status)
{
    for (;;) {
        for (int n = 1; n < NR_TASKS; n++) {
            if (task[n].state == TASK_ZOMBIE) {  /* day13: 收割任意僵尸 */
                if (status)
                    *status = task[n].exit_code;
                int pid = task[n].pid;
                task[n].state = TASK_DEAD;
                return pid;
            }
        }
        __asm__ volatile("hlt");
    }
}

/* ---- 调度(同 day11) ---- */
void schedule(void)
{
    int i, next = -1;
    for (i = 1; i <= NR_TASKS; i++) {
        int n = (int)(current - task + i) % NR_TASKS;
        if (task[n].state == TASK_RUNNABLE) {
            next = n;
            break;
        }
    }
    if (next < 0 || next == current - task) {
        for (i = 0; i < NR_TASKS; i++)
            if (task[i].state == TASK_RUNNABLE)
                task[i].counter = task[i].priority;
        return;
    }
    current->counter = current->priority;
    struct task_struct *prev = current;
    current = &task[next];
    current->counter = current->priority;
    /* ring3 用户任务被中断时 CPU 需要 TSS 换栈: esp0 → 新任务内核栈顶 */
    extern char system_tss[];
    ((volatile uint32_t *)system_tss)[1] =
        (uint32_t)(task_stack[next] + sizeof(task_stack[next]));
    /* 0.11: 每任务地址空间 —— CR3 与 LDTR 随任务切换 */
    mm_load_cr3(current->cr3);
    mm_load_ldt(current->ldt_sel);
    printk("[sw] %d->%d\n", (int)(prev - task), next);
    cpu_switch_to(&prev->kesp, current->kesp);
}

void do_timer(void)
{
    if (--current->counter <= 0)
        schedule();
}
