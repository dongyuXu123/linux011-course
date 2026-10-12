/* =====================================================================
 * day38 - user/files.c：用户态文件浏览器(列表版) —— day35 的 0.11
 * 文件 ABI(open=5/read=3/lseek=19/close=6) + day37 窗口 API 的合体:
 * 逐个打开已知文件, 读首行, 写进自己的窗口。
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

static char line[48];
static void show_line(long wid, const char *name)
{
    long fd = syscall3(5, (long)name, 0, 0);          /* open */
    int p = 0;
    line[p++] = '[';
    while (*name && p < 20) line[p++] = *name++;
    line[p++] = ']';
    line[p++] = ' ';
    if (fd >= 0) {
        long n = syscall3(3, fd, (long)(line + p), 24);  /* read */
        syscall3(6, fd, 0, 0);                            /* close */
        if (n < 0) n = 0;
        p += n;
        if (p > 0 && line[p-1] == '\n') p--;
    } else {
        line[p++] = '-';
        line[p++] = 'x';
    }
    line[p] = 0;
    syscall3(42, wid, (long)line, p);                     /* win_write */
}

__attribute__((section(".text.start"))) void _start(void)
{
    print("[day38] files: starting\n");
    long id = syscall5(41, 20, 200, 280, 110, (long)"files");
    print("[day38] files: win id ");
    static char pb[12];
    long v = id; int i = 0;
    if (v == 0) { pb[i++] = '0'; }
    while (v) { pb[i++] = '0' + v % 10; v /= 10; }
    pb[i] = 0;
    print(pb);
    print("\n");
    show_line(id, "motd");
    show_line(id, "notes.txt");
    show_line(id, "welcome.txt");
    print("[day38] files: done, exit\n");
    syscall3(1, 0, 0, 0);
}
