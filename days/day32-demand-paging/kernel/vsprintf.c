/* =====================================================================
 * day09 - kernel/vsprintf.c —— 移植自 Linux 0.11 kernel/vsprintf.c
 * ---------------------------------------------------------------------
 * 原作: Lars Wirzenius & Linus Torvalds, 1991
 * (C) 1991 Linus Torvalds —— 遵循原仓库 LICENSE 声明随课程分发
 *
 * 本课把它从 GCC 1.40 时代语法改写为现代 GCC 可编译版本:
 *   - do_div 宏的内联 asm 换成显式 64 位除法
 *   - stdarg 使用编译器内建
 * 支持: %[-0][宽度][.精度] d u x X o p s c %%
 * 对照阅读: source/kernel/vsprintf.c —— 结构一一对应
 * ===================================================================== */
#include <stdarg.h>
#include <stddef.h>
#include <string.h>

#define ZEROPAD  1   /* 前导 0 填充 */
#define SIGN     2   /* 有符号数 */
#define PLUS     4   /* 显示 + */
#define SPACE    8   /* 正数前留空格 */
#define LEFT     16  /* 左对齐 */
#define SPECIAL  32  /* 0x 前缀 */
#define SMALL    64  /* 小写十六进制 */

#define is_digit(c) ((c) >= '0' && (c) <= '9')

static int skip_atoi(const char **s)
{
    int i = 0;
    while (is_digit(**s))
        i = i * 10 + *((*s)++) - '0';
    return i;
}

/* 整数 → 字符串(number 子过程), 返回写到的位置 */
static char *number(char *str, unsigned int num, int base, int size,
                    int precision, int type)
{
    char c, sign, tmp[36];
    const char *digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    int i;

    if (type & SMALL)
        digits = "0123456789abcdefghijklmnopqrstuvwxyz";
    if (type & LEFT)
        type &= ~ZEROPAD;
    if (base < 2 || base > 36)
        return 0;

    c = (type & ZEROPAD) ? '0' : ' ';
    if (type & SIGN && (int)num < 0) {
        sign = '-';
        num = -(int)num;
    } else {
        sign = (type & PLUS) ? '+' : ((type & SPACE) ? ' ' : 0);
    }
    if (sign)
        size--;
    if (type & SPECIAL) {
        if (base == 16)
            size -= 2;
        else if (base == 8)
            size--;
    }
    i = 0;
    if (num == 0)
        tmp[i++] = '0';
    else
        while (num != 0) {
            tmp[i++] = digits[num % (unsigned)base];
            num /= (unsigned)base;
        }
    if (i > precision)
        precision = i;
    size -= precision;
    if (!(type & (ZEROPAD | LEFT)))
        while (size-- > 0)
            *str++ = ' ';
    if (sign)
        *str++ = sign;
    if (type & SPECIAL) {
        if (base == 8)
            *str++ = '0';
        else if (base == 16) {
            *str++ = '0';
            *str++ = digits[33];
        }
    }
    if (!(type & LEFT))
        while (size-- > 0)
            *str++ = c;
    while (i < precision--)
        *str++ = '0';
    while (i-- > 0)
        *str++ = tmp[i];
    while (size-- > 0)
        *str++ = ' ';
    return str;
}

int vsprintf(char *buf, const char *fmt, va_list args)
{
    int len;
    unsigned int num;
    int i, base;
    char *str;
    const char *s;
    int flags;

    int field_width;
    int precision;
    int qualifier;

    for (str = buf; *fmt; fmt++) {
        if (*fmt != '%') {
            *str++ = *fmt;
            continue;
        }
        /* process flags */
        flags = 0;
repeat:
        fmt++;
        switch (*fmt) {
        case '-': flags |= LEFT;    goto repeat;
        case '+': flags |= PLUS;    goto repeat;
        case ' ': flags |= SPACE;   goto repeat;
        case '#': flags |= SPECIAL; goto repeat;
        case '0': flags |= ZEROPAD; goto repeat;
        }

        /* get field width */
        field_width = -1;
        if (is_digit(*fmt))
            field_width = skip_atoi(&fmt);
        else if (*fmt == '*') {
            fmt++;
            field_width = va_arg(args, int);
            if (field_width < 0) {
                field_width = -field_width;
                flags |= LEFT;
            }
        }

        /* get the precision */
        precision = -1;
        if (*fmt == '.') {
            fmt++;
            if (is_digit(*fmt))
                precision = skip_atoi(&fmt);
            else if (*fmt == '*') {
                fmt++;
                precision = va_arg(args, int);
            }
            if (precision < 0)
                precision = 0;
        }

        /* get the conversion qualifier */
        qualifier = -1;
        if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L') {
            qualifier = *fmt;
            fmt++;
        }

        switch (*fmt) {
        case 'c':
            if (!(flags & LEFT))
                while (--field_width > 0)
                    *str++ = ' ';
            *str++ = (unsigned char)va_arg(args, int);
            while (--field_width > 0)
                *str++ = ' ';
            break;

        case 's':
            s = va_arg(args, char *);
            if (!s)
                s = "(null)";
            len = (int)strlen(s);
            if (precision < 0)
                precision = len;
            else if (len > precision)
                len = precision;
            if (!(flags & LEFT))
                while (len < field_width--)
                    *str++ = ' ';
            for (i = 0; i < len; i++)
                *str++ = *s++;
            while (len < field_width--)
                *str++ = ' ';
            break;

        case 'o':
            base = 8;
            goto number_case;
        case 'x':
            flags |= SMALL;
            /* fall through */
        case 'X':
            base = 16;
            goto number_case;
        case 'd':
        case 'i':
            flags |= SIGN;
            /* fall through */
        case 'u':
            base = 10;
            goto number_case;
        case 'p':
            if (field_width == -1) {
                field_width = 8;
                flags |= ZEROPAD;
            }
            *str++ = '0';
            *str++ = 'x';
            base = 16;
            goto number_case;

number_case:
            if (qualifier == 'l')
                num = va_arg(args, unsigned long);
            else if (qualifier == 'h') {
                num = (unsigned short)va_arg(args, int);
                if (flags & SIGN)
                    num = (short)num;
            } else if (flags & SIGN)
                num = va_arg(args, int);
            else
                num = va_arg(args, unsigned int);
            str = number(str, num, base, field_width, precision, flags);
            break;

        case '%':
            *str++ = '%';
            break;

        default:
            *str++ = '%';
            if (*fmt)
                *str++ = *fmt;
            else
                fmt--;
            break;
        }
    }
    *str = '\0';
    return str - buf;
}

int sprintf(char *buf, const char *fmt, ...)
{
    va_list args;
    int i;
    va_start(args, fmt);
    i = vsprintf(buf, fmt, args);
    va_end(args);
    return i;
}
