#ifndef _MATH_H_
#define _MATH_H_

#include"stdint.h"

static inline int imax(int a, int b) {
	return (a>b ? a : b);
}

static inline int imin(int a, int b) {
	return (a<b ? a : b);
}

static inline size_t umax(size_t a, size_t b) {
	return (a>b ? a : b);
}

static inline size_t umin(size_t a, size_t b) {
	return (a<b ? a : b);
}

static inline int slog2(size_t s) {
	int i=0;
	while(s) {
		i++;
		s >>= 1;
	}
	return i;
}

#endif
