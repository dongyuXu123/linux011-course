/* freestanding 串/内存操作 —— mk/include/string.h 的实现 */
#include <string.h>

size_t strlen(const char *s)
{
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

char *strcpy(char *dst, const char *src)
{
    char *p = dst;
    while ((*p++ = *src++))
        ;
    return dst;
}

char *strncpy(char *dst, const char *src, size_t n)
{
    size_t i = 0;
    for (; i < n && src[i]; i++)
        dst[i] = src[i];
    for (; i < n; i++)
        dst[i] = 0;
    return dst;
}

char *strcat(char *dst, const char *src)
{
    char *p = dst;
    while (*p) p++;
    while ((*p++ = *src++))
        ;
    return dst;
}

void *memset(void *s, int c, size_t n)
{
    unsigned char *p = s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

void *memcpy(void *d, const void *s, size_t n)
{
    unsigned char *pd = d;
    const unsigned char *ps = s;
    while (n--) *pd++ = *ps++;
    return d;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *pa = a, *pb = b;
    while (n--) {
        if (*pa != *pb)
            return *pa - *pb;
        pa++; pb++;
    }
    return 0;
}
