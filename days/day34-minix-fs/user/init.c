/* =====================================================================
 * day19 - user/init.c：第一个用户态 init(对应 0.11 的 /bin/init 形态)
 * 编译: gcc -m32 -ffreestanding -nostdlib -Ttext=0x40000 (无 libc)
 * 系统调用 ABI 沿用 0.11: fork=2 read=3 write=4 execve=11 wait4=72
 * ===================================================================== */

static long syscall3(long n, long a, long b, long c)
{
    long r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(n), "b"(a), "c"(b), "d"(c));
    return r;
}

static int slen(const char *s)
{
    int n = 0;
    while (s[n]) n++;
    return n;
}

static void print(const char *s)
{
    syscall3(4, 1, (long)s, slen(s));
}

static char pb[12];          /* 课程简化: 用静态缓冲存放数字(避免依赖
                                用户栈页在内核侧的可见性差异) */

static void printnum(int v)
{
    char *b = pb;
    int i = 0;
    if (v == 0) { print("0"); return; }
    if (v < 0) { print("-"); v = -v; }
    while (v) { b[i++] = '0' + v % 10; v /= 10; }
    while (i) syscall3(4, 1, (long)(char *)&b[--i], 1);
}

static int starts(const char *s, const char *pre)
{
    while (*pre)
        if (*s++ != *pre++)
            return 0;
    return 1;
}

__attribute__((section(".text.start"))) void _start(void)
{
    print("[day34] /sbin/init running in ring 3 (pid ");
    printnum(syscall3(20, 0, 0, 0));
    print(")\n");

    static char line[64];
    for (;;) {
        print("init> ");
        long n = syscall3(3, 0, (long)line, 63);      /* read: 行模式, 阻塞 */
        if (n <= 0)
            continue;
        if (line[n - 1] == '\n')
            line[n - 1] = 0;
        else
            line[n] = 0;

        if (starts(line, "run ")) {
            long pid = syscall3(2, 0, 0, 0);    /* fork */
            if (pid == 0) {
                syscall3(11, (long)(line + 4), 0, 0);  /* execve */
                print("exec failed\n");
                syscall3(1, 1, 0, 0);                  /* exit(1) */
            }
            print("[init] forked child pid ");
            printnum(pid);
            print("\n");
            static int st;
            long w = syscall3(72, pid, (long)(int *)&st, 0);  /* wait4 */
            print("[init] child ");
            printnum(w);
            print(" exited with ");
            printnum(st);
            print("\n");
        } else {
            print("init: unknown command (try 'run hello')\n");
        }
    }
}
