// Must keep in sync with ../libvxc/include

#include <sys/types.h>
#include <stdint.h>

#define VXC_MAXNAMLEN 255

// DO NOT USE "long" in this file - it needs to behave same on 32 and 64-bit.
#define long __no_long_here_thanks

typedef int32_t		vxc_ssize_t;

typedef int32_t		vxc_mode_t;
typedef int32_t		vxc_off_t;
typedef uint32_t	vxc_dev_t;
typedef uint32_t	vxc_ino_t;
typedef int32_t		vxc_nlink_t;
typedef int32_t		vxc_blksize_t;
typedef int32_t		vxc_blkcnt_t;

typedef int32_t		vxc_id_t;
typedef int32_t		vxc_pid_t; // XXX where else is this getting defined?
typedef int32_t		vxc_uid_t;
typedef int32_t		vxc_gid_t;

typedef int32_t		vxc_time_t;
typedef int32_t		vxc_clock_t;
typedef int32_t		vxc_suseconds_t;


struct vxc_dirent {
	uint32_t d_fileno;
	uint16_t d_reclen;
	uint8_t d_type;
	uint8_t d_namlen;
	char	d_name[VXC_MAXNAMLEN + 1];
};

struct vxc_tm {
	int    tm_sec;
	int    tm_min;
	int    tm_hour;
	int    tm_mday;
	int    tm_mon;
	int    tm_year;
	int    tm_wday;
	int    tm_yday;
	int    tm_isdst;
};

struct vxc_utimbuf {
	vxc_time_t actime;
	vxc_time_t modtime;
}; 

struct vxc_timeval {
	vxc_time_t		tv_sec;		// Seconds
	vxc_suseconds_t	tv_usec;	// Microseconds
};

struct vxc_itimerval {
	struct vxc_timeval	it_interval;	// Timer interval
	struct vxc_timeval	it_value;	// Current value
};

struct vxc_stat
{
	vxc_dev_t	dev;		// Device ID of device containing file. 
	vxc_ino_t	ino;		// File serial number. 
	vxc_mode_t	mode;	// Mode of file.
	vxc_nlink_t	nlink;	// Number of hard links to the file. 
	vxc_uid_t	uid;		// User ID of file. 
	vxc_gid_t	gid;		// Group ID of file. 
	vxc_dev_t	rdev;	// Device ID for device special file.
	vxc_blksize_t blksize;	// Size of blocks in this file
	vxc_blkcnt_t blocks;	// Number of blocks allocated for this file
	vxc_off_t	size;	// File size in bytes.
	vxc_time_t	atime;	// Time of last access. 
	vxc_time_t	mtime;	// Time of last data modification. 
	vxc_time_t	ctime;	// Time of last status change. 
};


// File mode bits - matching vx/env.h
#define VXC_S_IFMT		070000		// File type bit mask
#define VXC_S_IFNULL	000000		// Unused inode
#define VXC_S_IFREG		010000		// Regular file
#define VXC_S_IFDIR		020000		// Directory
#define VXC_S_IFLNK		030000		// Symbolic link
#define VXC_S_IFSOCK	040000		// Socket
#define VXC_S_IFIFO		050000		// FIFO
#define VXC_S_IFBLK		060000		// Block device
#define VXC_S_IFCHR		070000		// Character device

#define VXC_S_ISREG(m)	(((m) & VXC_S_IFMT) == VXC_S_IFREG)
#define VXC_S_ISDIR(m)	(((m) & VXC_S_IFMT) == VXC_S_IFDIR)
#define VXC_S_ISLNK(m)	(((m) & VXC_S_IFMT) == VXC_S_IFLNK)
#define VXC_S_ISSOCK(m)	(((m) & VXC_S_IFMT) == VXC_S_IFSOCK)
#define VXC_S_ISFIFO(m)	(((m) & VXC_S_IFMT) == VXC_S_IFIFO)
#define VXC_S_ISBLK(m)	(((m) & VXC_S_IFMT) == VXC_S_IFBLK)
#define VXC_S_ISCHR(m)	(((m) & VXC_S_IFMT) == VXC_S_IFCHR)

#define VXC_S_ISUID		004000
#define VXC_S_ISGID		002000
#define VXC_S_ISVTX		001000

#define VXC_S_IRWXU		000700
#define VXC_S_IRUSR		000400
#define VXC_S_IWUSR		000200
#define VXC_S_IXUSR		000100
                         
#define VXC_S_IRWXG		000070
#define VXC_S_IRGRP		000040
#define VXC_S_IWGRP		000020
#define VXC_S_IXGRP		000010
                         
#define VXC_S_IRWXO		000007
#define VXC_S_IROTH		000004
#define VXC_S_IWOTH		000002
#define VXC_S_IXOTH		000001

// File access modes
#define VXC_O_ACCMODE	0x03	// Mask for file access modes.
#define VXC_O_RDONLY	0x00	// Open for reading only.
#define VXC_O_WRONLY	0x01	// Open for writing only. 
#define VXC_O_RDWR		0x02	// Open for reading and writing.

// File creation flags
#define VXC_O_CREAT		0x10	// Create file if it does not exist.
#define VXC_O_EXCL		0x20	// Exclusive use flag.
#define VXC_O_NOCTTY	0x40	// Do not assign controlling terminal.
#define VXC_O_TRUNC		0x80	// Truncate flag. 

// File access flags
#define VXC_O_APPEND	0x100	// Set append mode.
#define VXC_O_NONBLOCK	0x200	// Non-blocking mode.
#define VXC_O_SYNC		0x400	// Synchronous I/O.

// Fcntl args
#define	VXC_F_DUPFD		0
#define	VXC_F_GETFD		1
#define	VXC_F_SETFD		2
#define	VXC_F_GETFL		3
#define	VXC_F_SETFL		4
// #define	VXC_F_GETLK		7
// #define	VXC_F_SETLK		8
// #define	VXC_F_SETLKW	9

#define	VXC_FD_CLOEXEC	1

