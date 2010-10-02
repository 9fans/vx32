#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include "syscall.h"

int execl(const char *path, const char *arg, ...)
{
	return execv(path, (char *const*)&arg);
}

int execv(const char *path, char *const arg[])
{
	return execve(path, arg, environ);
}

int execve(const char *path, char *const arg[], char *const env[])
{
	return syscall(VXSYSEXEC, (int)path, (int)arg, (int)env, 0, 0);
}

int execlp(const char *path, const char *arg, ...)
{
	return execvp(path, (char*const*)&arg);
}

int execvp(const char *file, char *const arg[])
{
	struct stat st;

	if(file[0] == '/' || (file[0] == '.' && file[1] == '/'))
		return execv(file, arg);

	char buf[1024];
	char *path = getenv("PATH");
	if(path == NULL){
		errno = ENOENT;
		return -1;
	}
	char *p, *ep;
	int n;
	for(p=path; p; p=ep){
		ep = strchr(p, ':');
		if(ep)
			n = ep++ - p;
		else
			n = strlen(p);
		if(n+1+strlen(file)+1 > sizeof buf)
			continue;
		if(n == 0)
			strcpy(buf, file);
		else{
			memmove(buf, p, n);
			buf[n] = '/';
			strcpy(buf+n+1, file);
		}
		if(stat(buf, &st) >= 0)
			return execv(buf, arg);
	}
	errno = ENOENT;
	return -1;
}
