#ifndef _SYS_STAT_H
#define _SYS_STAT_H

#include <sys/types.h>

struct stat
{
	dev_t	st_dev;		// Device ID of device containing file. 
	ino_t	st_ino;		// File serial number. 
	mode_t	st_mode;	// Mode of file.
	nlink_t	st_nlink;	// Number of hard links to the file. 
	uid_t	st_uid;		// User ID of file. 
	gid_t	st_gid;		// Group ID of file. 
	dev_t	st_rdev;	// Device ID for device special file.
	blksize_t st_blksize;	// Size of blocks in this file
	blkcnt_t st_blocks;	// Number of blocks allocated for this file
	off_t	st_size;	// File size in bytes.
	time_t	st_atime;	// Time of last access. 
	time_t	st_mtime;	// Time of last data modification. 
	time_t	st_ctime;	// Time of last status change. 
};


// File mode bits - matching vx/env.h
#define S_IFMT		070000		// File type bit mask
#define S_IFNULL	000000		// Unused inode
#define S_IFREG		010000		// Regular file
#define S_IFDIR		020000		// Directory
#define S_IFLNK		030000		// Symbolic link
#define S_IFSOCK	040000		// Socket
#define S_IFIFO		050000		// FIFO
#define S_IFBLK		060000		// Block device
#define S_IFCHR		070000		// Character device

#define S_ISREG(m)	(((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m)	(((m) & S_IFMT) == S_IFDIR)
#define S_ISLNK(m)	(((m) & S_IFMT) == S_IFLNK)
#define S_ISSOCK(m)	(((m) & S_IFMT) == S_IFSOCK)
#define S_ISFIFO(m)	(((m) & S_IFMT) == S_IFIFO)
#define S_ISBLK(m)	(((m) & S_IFMT) == S_IFBLK)
#define S_ISCHR(m)	(((m) & S_IFMT) == S_IFCHR)

#define S_ISUID		004000
#define S_ISGID		002000
#define S_ISVTX		001000

#define S_IRWXU		000700
#define S_IRUSR		000400
#define S_IWUSR		000200
#define S_IXUSR		000100
                         
#define S_IRWXG		000070
#define S_IRGRP		000040
#define S_IWGRP		000020
#define S_IXGRP		000010
                         
#define S_IRWXO		000007
#define S_IROTH		000004
#define S_IWOTH		000002
#define S_IXOTH		000001

#define	DEFFILEMODE	(S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)

int stat(const char *file_name, struct stat *buf);
int fstat(int filedes, struct stat *buf);
int lstat(const char *file_name, struct stat *buf);

int chmod(const char *path, mode_t mode);
int fchmod(int fd, mode_t mode);

int mkdir(const char *path, mode_t mode);
int mkfifo(const char *path, mode_t mode);
int mknod(const char *path, mode_t mode);

mode_t umask(mode_t cmask);


#endif	// _SYS_STAT_H
