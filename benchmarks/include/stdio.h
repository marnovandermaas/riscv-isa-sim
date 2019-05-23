#ifndef __STDIO_H__
#define __STDIO_H__

#include"stdint.h"
#include"stdarg.h"

typedef void FILE;

int	kvprintf(char const *fmt, void (*func)(int, void*), void *arg, int radix, va_list ap);
int	vsprintf(char *buf, const char *cfmt, va_list ap);
int	vsnprintf(char *str, size_t size, const char *format, va_list ap);
int	printf(const char *fmt, ...);
int	vprintf(const char *fmt, va_list ap);
int	puts(const char *s);
int putchar(int character);

#endif /* !__STDIO_H__ */
