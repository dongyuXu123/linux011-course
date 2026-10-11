/* freestanding string.h —— 内核自持 */
#ifndef _STRING_H
#define _STRING_H
#include <stddef.h>
size_t strlen(const char *s);
int    strcmp(const char *a, const char *b);
char  *strcpy(char *dst, const char *src);
char  *strncpy(char *dst, const char *src, size_t n);
char  *strcat(char *dst, const char *src);
void  *memset(void *s, int c, size_t n);
void  *memcpy(void *d, const void *s, size_t n);
int    memcmp(const void *a, const void *b, size_t n);
#endif
