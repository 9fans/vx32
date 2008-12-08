#include	"u.h"
#include	"lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"error.h"

#include "authsrv.h"

char	*eve;
char	hostdomain[DOMLEN];

/*
 *  return true if current user is eve
 */
int
iseve(void)
{
	return strcmp(eve, up->user) == 0;
}

long
sysfversion(u32int *arg)
{
	char *vers;
	uint arglen, m, msize;
	Chan *c;

	msize = arg[1];
	arglen = arg[3];
	vers = uvalidaddr(arg[2], arglen, 1);
	/* check there's a NUL in the version string */
	if(arglen==0 || memchr(vers, 0, arglen)==0)
		error(Ebadarg);
	c = fdtochan(arg[0], ORDWR, 0, 1);
	if(waserror()){
		cclose(c);
		nexterror();
	}

	m = mntversion(c, vers, msize, arglen);

	cclose(c);
	poperror();
	return m;
}

long
sys_fsession(u32int *arg)
{
	/* deprecated; backwards compatibility only */

	if(arg[2] == 0)
		error(Ebadarg);
	*(char*)uvalidaddr(arg[1], arg[2], 1) = '\0';
	return 0;
}

long
sysfauth(u32int *arg)
{
	Chan *c, *ac;
	char *aname;
	int fd;

	aname = validnamedup(uvalidaddr(arg[1], 1, 0), 1);
	if(waserror()){
		free(aname);
		nexterror();
	}
	c = fdtochan(arg[0], ORDWR, 0, 1);
	if(waserror()){
		cclose(c);
		nexterror();
	}

	ac = mntauth(c, aname);
	/* at this point ac is responsible for keeping c alive */
	cclose(c);
	poperror();	/* c */
	free(aname);
	poperror();	/* aname */

	if(waserror()){
		cclose(ac);
		nexterror();
	}

	fd = newfd(ac);
	if(fd < 0)
		error(Enofd);
	poperror();	/* ac */

	/* always mark it close on exec */
	ac->flag |= CCEXEC;

	return fd;
}

/*
 *  called by devcons() for user device
 *
 *  anyone can become none
 */
long
userwrite(char *a, int n)
{
	if(n!=4 || strncmp(a, "none", 4)!=0)
		error(Eperm);
	kstrdup(&up->user, "none");
	up->basepri = PriNormal;
	return n;
}

/*
 *  called by devcons() for host owner/domain
 *
 *  writing hostowner also sets user
 */
long
hostownerwrite(char *a, int n)
{
	char buf[128];

	if(!iseve())
		error(Eperm);
	if(n <= 0 || n >= sizeof buf)
		error(Ebadarg);
	memmove(buf, a, n);
	buf[n] = 0;

	renameuser(eve, buf);
	kstrdup(&eve, buf);
	kstrdup(&up->user, buf);
	up->basepri = PriNormal;
	return n;
}

long
hostdomainwrite(char *a, int n)
{
	char buf[DOMLEN];

	if(!iseve())
		error(Eperm);
	if(n >= DOMLEN)
		error(Ebadarg);
	memset(buf, 0, DOMLEN);
	strncpy(buf, a, n);
	if(buf[0] == 0)
		error(Ebadarg);
	memmove(hostdomain, buf, DOMLEN);
	return n;
}
