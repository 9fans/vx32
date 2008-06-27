#ifndef _DIRENT_H_
#define _DIRENT_H_

typedef struct DIR DIR;
struct DIR {
	int fd;
};

#define	MAXNAMLEN	255

#include <stdint.h>

struct dirent {
	uint32_t d_fileno;
	uint16_t d_reclen;
	uint8_t d_type;
	uint8_t d_namlen;
	char	d_name[MAXNAMLEN + 1];
};

#define	DT_UNKNOWN	 0
#define	DT_FIFO		 1
#define	DT_CHR		 2
#define	DT_DIR		 4
#define	DT_BLK		 6
#define	DT_REG		 8
#define	DT_LNK		10
#define	DT_SOCK		12
#define	DT_WHT		14


DIR *opendir(const char*);
struct dirent *readdir(DIR*);
long telldir(DIR*);
void seekdir(DIR*, long);
void rewinddir(DIR*);
int closedir(DIR*);
int dirfd(DIR*);

#endif  // _DIRENT_H_
