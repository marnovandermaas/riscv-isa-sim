#ifndef _STDIO_H
#define _STDIO_H

#include <stdarg.h>
#include <stddef.h>

typedef signed char __int8_t;
typedef unsigned char __uint8_t;
typedef signed short int __int16_t;
typedef unsigned short int __uint16_t;
typedef signed int __int32_t;
typedef unsigned int __uint32_t;
typedef signed long int __int64_t;
typedef unsigned long int __uint64_t;
typedef __uint8_t uint8_t;
typedef __uint16_t uint16_t;
typedef __uint32_t uint32_t;
typedef __uint64_t uint64_t;



//#define stdin 1
//#define stdout 2 
//#define stderr 3

#define static_assert(cond) switch(0) { case 0: case !!(long)(cond): ; }

#ifndef BUFSIZ
# define BUFSIZ 1024//_IO_BUFSIZ
#endif

struct _FILE;
typedef struct _FILE {
    size_t (*write)(struct _FILE *f, const char *, size_t);
    char *wpos;
    char *wend;
} FILE;
int fflush(FILE *);
int fprintf(FILE *, const char *, ...);
int printf(const char *, ...);
int rename(const char *, const char *);

int sscanf(const char *, const char *, ...);
int snprintf(char *, size_t, const char *, ...);
int vfprintf(FILE *, const char *, va_list);
int vsnprintf(char *, size_t, const char *, va_list);
/*
 * The following definitions are not required by the OCaml runtime, but are
 * needed to build the freestanding version of GMP used by Mirage.
 */
#define EOF (-1)
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;
size_t fread(void *, size_t, size_t, FILE *);
int getc(FILE *);
int ungetc(int, FILE *);
size_t fwrite(const void *, size_t, size_t, FILE *);
int fputc(int, FILE *);
int putc(int, FILE *);
int ferror(FILE *);
extern int puts(const char *string);



#endif
