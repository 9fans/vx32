#ifndef _SYS_TYPES_H
#define _SYS_TYPES_H

#include <stddef.h>

typedef signed long		ssize_t;

typedef int			mode_t;
typedef signed long long	off_t;
typedef unsigned long		dev_t;
typedef unsigned long		ino_t;
typedef int			nlink_t;
typedef signed			blksize_t;
typedef signed long long	blkcnt_t;

typedef int			id_t;
typedef int			pid_t; // XXX where else is this getting defined?
typedef int			uid_t;
typedef int			gid_t;

typedef signed long long	time_t;
typedef signed long long	clock_t;
typedef signed long		suseconds_t;

struct tm;
#include <fcntl.h>  // I give up!  Some header is supposed to define O_RDONLY
	// as a side-effect but I don't know which one.

#endif	// _SYS_TYPES_H
