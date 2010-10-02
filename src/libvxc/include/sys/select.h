#ifndef _SYS_SELECT_H_
#define _SYS_SELECT_H_

#define FD_SETSIZE 32
typedef unsigned int fd_set;

struct timeval;
int select(int, fd_set*, fd_set*, fd_set*, struct timeval*);

#define FD_SET(fd, s)  ((*s) |= (1<<(fd)))
#define FD_CLR(fd, s) ((*s) &= ~(1<<(fd)))
#define FD_ISSET(fd, s) (((*s) & (1<<(fd))) != 0)
#define FD_ZERO(s) ((*s) = 0)

#endif  // _SYS_SELECT_H_
