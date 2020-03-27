#ifndef _UNISTD_H
#define _UNISTD_H

#include <stdint.h>

int chdir(const char *);
int close(int);
char *getcwd(char *, size_t);
typedef int pid_t;
pid_t getpid(void);
pid_t getppid(void);
int isatty(int);
typedef int off_t;
off_t lseek(int, off_t, int); /* SEEK_ */
ssize_t read(int, void *, size_t);
ssize_t write(int, const void *, size_t);
ssize_t readlink(const char *, char *, size_t);
int unlink(const char *);

#endif