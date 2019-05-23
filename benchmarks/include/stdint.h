#ifndef _STDINT_H_
#define	_STDINT_H_


#define REG_SIZE 8

/*
 * 64-bit MIPS types.
 */
typedef unsigned long	register_t;		/* 64-bit MIPS register */
typedef unsigned long	paddr_t;		/* Physical address */
typedef unsigned long	vaddr_t;		/* Virtual address */

typedef long		ssize_t;
typedef	unsigned long	size_t;

typedef long		off_t;

/*
 * Useful integer type names that we can't pick up from the compile-time
 * environment.
 */
typedef char		int8_t;
typedef unsigned char	u_char;
typedef unsigned char	uint8_t;
typedef short		int16_t;
typedef unsigned short	u_short;
typedef unsigned short	uint16_t;
typedef int		int32_t;
typedef unsigned int	u_int;
typedef unsigned int	uint32_t;
typedef long		intmax_t;
typedef long		quad_t;
typedef long		ptrdiff_t;
typedef long		int64_t;
typedef unsigned long	u_long;
typedef unsigned long	uint64_t;
typedef	unsigned long	uintmax_t;
typedef unsigned long	u_quad_t;
//typedef unsigned long	uintptr_t;
typedef unsigned long	caddr_t;

typedef u_long		ulong;
typedef u_char		uchar;
typedef uint8_t		u8;
typedef uint16_t	u16;
typedef uint32_t	u32;
typedef uint64_t	u64;
typedef uint16_t	__u16;
typedef uint32_t	__u32;
typedef uint8_t		u_int8_t;
typedef uint16_t	u_int16_t;
typedef uint32_t	u_int32_t;
typedef uint64_t	u_int64_t;
#define UCHAR_MAX   255
#define ULONG_MAX	4294967295UL
#define UINT_MAX	ULONG_MAX
#define CHAR_BIT	8

#define	NBBY		8	/* Number of bits per byte. */
#define	NULL		((void *)0)

#endif /* _STDINT_H_ */
