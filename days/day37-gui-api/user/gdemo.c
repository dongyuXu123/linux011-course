/* =====================================================================
 * day37 - user/gdemo.c：第一个"经系统调用建窗"的 ring3 图形程序
 * ---------------------------------------------------------------------
 * 30dayMakeOS 的 API 思想 + 0.11 的进程模型: 窗口不是内核画的,
 * 是应用经 int 0x80 申请的; 内核只提供合成。
 * win_create=41(x,y,w,h,name 五参走 ebx/ecx/edx/esi/edi)
 * win_write=42(id, buf, len)
 * ===================================================================== */

static long syscall3(long n, long a, long b, long c)
{
    long r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(n), "b"(a), "c"(b), "d"(c));
    return r;
}

static long syscall5(long n, long a, long b, long c, long d, long e)
{
    long r;
    __asm__ volatile("int $0x80"
                     : "=a"(r)
                     : "a"(n), "b"(a), "c"(b), "d"(c), "S"(d), "D"(e));
    return r;
}

static void print(const char *s)
{
    long n = 0;
    while (s[n]) n++;
    syscall3(4, 1, (long)s, n);
}

static char pb[12];
static void printnum(long v)
{
    int i = 0;
    if (v == 0) { print("0"); return; }
    while (v) { pb[i++] = '0' + v % 10; v /= 10; }
    while (i) syscall3(4, 1, (long)(char *)&pb[--i], 1);
}

__attribute__((section(".text.start"))) void _start(void)
{
    print("[day37] gdemo: creating window\n");
    long id = syscall5(41, 320, 60, 220, 90, (long)"gdemo");
    print("[day37] gdemo: win id ");
    printnum(id);
    print("\n");
    syscall3(42, id, (long)"hello from ring3 gui", 20);
    syscall3(1, 0, 0, 0);   /* exit(0) */
}
