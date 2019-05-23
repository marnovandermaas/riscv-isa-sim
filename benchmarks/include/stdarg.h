#ifndef _STDARG_H_
#define	_STDARG_H_

/*
 * Minimalist contents of a traditional stdarg.h required to use printf().
 */

typedef __builtin_va_list	va_list;
#define	va_start(v, l)		__builtin_va_start((v), l)
#define	va_end			__builtin_va_end
#define	va_arg			__builtin_va_arg
#define	va_copy			__builtin_va_copy

#endif /* _STDARG_H_ */
