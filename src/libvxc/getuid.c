#include <unistd.h>
#include <errno.h>

pid_t getpid(void) { return 1; }
uid_t getuid(void) { return 0; }
uid_t geteuid(void) { return 0; }
gid_t getgid(void) { return 0; }
gid_t getegid(void) { return 0; }

int setuid(uid_t uid) { errno = EINVAL; return -1; }
int setgid(gid_t gid) { errno = EINVAL; return -1; }
