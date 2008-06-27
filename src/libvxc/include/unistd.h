#ifndef _UNISTD_H_
#define _UNISTD_H_

#include <stdint.h>
#include <sys/types.h>

#define STDIN_FILENO	0
#define STDOUT_FILENO	1
#define STDERR_FILENO	2

#ifndef SEEK_SET
#define SEEK_SET	0
#define SEEK_CUR	1
#define SEEK_END	2
#endif


// File and directory operations
int unlink(const char *path);
int remove(const char *path);
int rmdir(const char *path);

// File I/O
ssize_t read(int fd, void *buf, size_t size);
ssize_t write(int fd, const void *buf, size_t size);
off_t lseek(int fildes, off_t offset, int whence);

// File descriptor handling
int dup(int oldfd);
int dup2(int oldfd, int newfd);
void *sbrk(intptr_t increment);
int close(int);

void _exit(int status);
int execl(const char *path, const char *arg, ...);
int execv(const char *path, char *const argv[]);
int execve(const char *path, char *const argv[], char *const env[]);
int execlp(const char *path, const char *arg, ...);
int execvp(const char *path, char *const argv[]);

extern char **environ;

char *getcwd(char*, size_t);

uid_t getuid(void);
uid_t geteuid(void);
gid_t getgid(void);
gid_t getegid(void);
pid_t getpid(void);

pid_t fork(void);

#endif	// _UNISTD_H_
