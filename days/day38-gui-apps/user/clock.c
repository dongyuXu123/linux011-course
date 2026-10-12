/* =====================================================================
 * day38 - user/clock.c：用户态图形时钟 —— 30dayMakeOS day21 的课程对应
 * ring3 经 win_create/win_write 画自己的窗口, time=13(0.11 同号)
 * 取秒; 5 秒后退出(课程测试需要可终止的应用)。
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

static void win_show(long id, long secs, long up)
{
    /* 窗口文字: "uptime NNs (t=NNs)" 直接写进内容区 */
    static char line[32];
    char *p = line;
    *p++ = 'u'; *p++ = 'p'; *p++ = 't'; *p++ = 'i'; *p++ = 'm'; *p++ = 'e';
    *p++ = ' ';
    if (up >= 10) *p++ = '0' + up / 10;
    *p++ = '0' + up % 10;
    *p++ = 's';
    *p = 0;
    syscall3(42, id, (long)line, p - line);
}

__attribute__((section(".text.start"))) void _start(void)
{
    print("[day38] clock: starting\n");
    long id = syscall5(41, 330, 160, 200, 90, (long)"clock");
    print("[day38] clock: win id ");
    printnum(id);
    print("\n");
    long last = -1, ups = 0;
    for (long t = 0; ups < 5; t = syscall3(13, 0, 0, 0)) {
        if (t != last) {
            last = t;
            win_show(id, t, ups);
            ups++;
        }
    }
    print("[day38] clock: 5 ticks shown, exit\n");
    syscall3(1, 0, 0, 0);
}
