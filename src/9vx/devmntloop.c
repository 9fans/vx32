/*
 * Clumsy hack to be able to create mountable files in /srv that
 * correspond to certain directories, kind of like an in-kernel exportfs:
 *
 *	fd = open("#~/mntloop", ORDWR);
 *	fprint(fd, "/mnt/foo");
 *	sfd = create("/srv/foo", ORDWR, 0666);
 *	fprint(sfd, "%d", fd);
 *	close(sfd);
 *	close(fd);
 *
 * is almost equivalent to
 *
 *	srvfs /mnt/foo foo
 *
 * but avoids the translation to 9P and back when you later
 * mount /srv/foo.  There are a few inaccuracies compared
 * to what srvfs does:
 *
 * 	binds and mounts inside the tree rooted at /mnt/foo
 *	in the original name space are not present when a
 *	different name space mounts /srv/foo.
 *
 *	if the exported tree is a kernel device, then the kernel
 *	device will use the name of the user who mounted /srv/foo
 *	(not the name of the user who exported it) for permissions checks.
 *
 * This is all so that we can provide a /srv/boot file even if the
 * root is from a kernel device.  It's not intended for general use.
 */

#include	"u.h"
#include	"lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"error.h"

enum {
	Qdir = 0,
	Qmntloop = 1
};

static Chan*
mntloopattach(char *spec)
{
	Chan *c;
	c = devattach('~', spec);
	mkqid(&c->qid, Qdir, 0, QTDIR);
	return c;
}

static Dirtab dir[] = {
	"#~",	{Qdir, 0, QTDIR},	0,	DMDIR|0555,
	"mntloop",	{Qmntloop, 0, 0},	0,	0666,
};

static int
mntloopgen(Chan *c, char *name, Dirtab *tab, int ntab, int s, Dir *dp)
{
	if(s == DEVDOTDOT){
		devdir(c, c->qid, "#~", 0, eve, DMDIR|0555, dp);
		return 1;
	}
	
	return devgen(c, name, dir, nelem(dir), s, dp);
}

static Walkqid*
mntloopwalk(Chan *c, Chan *nc, char **name, int nname)
{
	return devwalk(c, nc, name, nname, nil, 0, mntloopgen);
}

static int
mntloopstat(Chan *c, uchar *dp, int n)
{
	return devstat(c, dp, n, nil, 0, mntloopgen);
}

static Chan*
mntloopopen(Chan *c, int omode)
{
	return devopen(c, omode, nil, 0, mntloopgen);
}

static void
mntloopclose(Chan *c)
{
	if(c->aux)
		cclose(c->aux);
}

static long
mntloopread(Chan *c, void *va, long n, vlong off)
{
	error(Egreg);
	return -1;
}

static long
mntloopwrite(Chan *c, void *va, long n, vlong off)
{
	char *p;
	Chan *nc;

	if(c->aux || off != 0 || n >= BY2PG)
		error(Ebadarg);

	p = smalloc(n+1);
	memmove(p, va, n);
	p[n] = 0;
	if(waserror()){
		free(p);
		nexterror();
	}
	nc = namec(p, Atodir, 0, 0);
	free(p);
	poperror();
	lock(&c->ref.lk);
	if(c->aux){
		unlock(&c->ref.lk);
		cclose(nc);
		error(Ebadarg);
	}
	c->aux = nc;
	unlock(&c->ref.lk);
	return n;
}

Dev mntloopdevtab = {	/* known to mntattach */
	'~',
	"mntloop",
	
	devreset,
	devinit,
	devshutdown,
	mntloopattach,
	mntloopwalk,
	mntloopstat,
	mntloopopen,
	devcreate,
	mntloopclose,
	mntloopread,
	devbread,
	mntloopwrite,
	devbwrite,
	devremove,
	devwstat,
};

