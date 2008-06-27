#include <dirent.h>
#include <errno.h>
#include <stdio.h>

DIR *opendir(const char *path)
{
	errno = EINVAL;
	return NULL;
}

struct dirent *readdir(DIR *dir)
{
	return NULL;
}

long telldir(DIR *dir) 
{
	return 0;
}

void seekdir(DIR *dir, long loc)
{
}

void rewinddir(DIR *dir)
{
}

int closedir(DIR *dir)
{
	return -1;
}

int dirfd(DIR *dir)
{
	return -1;
}
