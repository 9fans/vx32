#ifndef _FCNTL_H
#define _FCNTL_H

#include <sys/types.h>


// File access modes
#define O_ACCMODE	0x03	// Mask for file access modes.
#define O_RDONLY	0x00	// Open for reading only.
#define O_WRONLY	0x01	// Open for writing only. 
#define O_RDWR		0x02	// Open for reading and writing.

// File creation flags
#define O_CREAT		0x10	// Create file if it does not exist.
#define O_EXCL		0x20	// Exclusive use flag.
#define O_NOCTTY	0x40	// Do not assign controlling terminal.
#define O_TRUNC		0x80	// Truncate flag. 

// File access flags
#define O_APPEND	0x100	// Set append mode.
#define O_NONBLOCK	0x200	// Non-blocking mode.
#define O_SYNC		0x400	// Synchronous I/O.

// Fcntl args
#define	F_DUPFD		0
#define	F_GETFD		1
#define	F_SETFD		2
#define	F_GETFL		3
#define	F_SETFL		4
// #define	F_GETLK		7
// #define	F_SETLK		8
// #define	F_SETLKW	9

#define	FD_CLOEXEC	1



int creat(const char *path, mode_t mode);
int fcntl(int fd, int cmd, ...);
int open(const char *path, int flags, ...);


#endif	// _FCNTL_H
