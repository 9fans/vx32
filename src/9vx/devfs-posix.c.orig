#include	"u.h"
#include	<stdio.h> /* for remove, rename */
#include	<pwd.h>
#include	<grp.h>	/* going to regret this - getgrgid is a stack smasher */
#include	<sys/socket.h>
#include	<sys/un.h>

#if defined(__FreeBSD__)
#include	<sys/disk.h>
#include	<sys/disklabel.h>
#include	<sys/ioctl.h>
#endif

#if defined(__APPLE__)
#include	<sys/disk.h>
#endif

#if defined(__linux__)
#include	<linux/hdreg.h>
#include	<linux/fs.h>
#include	<sys/ioctl.h>
#endif

#include	"lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"error.h"

enum
{
	Trace = 0,
	FsChar = 'Z',
};

extern Path *addelem(Path*, char*, Chan*);
char	*localroot = "/home/rsc/plan9/4e";

static char *uidtoname(int);
static char *gidtoname(int);
static int nametouid(char*);
static int nametogid(char*);

static vlong disksize(int, struct stat*);

typedef struct UnixFd UnixFd;
struct UnixFd
{
	int	fd;
	int	issocket;
	int	plan9;	// rooted at localroot?
	DIR*	dir;
	vlong	diroffset;
	QLock	dirlock;
	struct dirent *nextde;
	Path	*path;	// relative to localroot
};

void
oserrstr(void)
{
	kstrcpy(up->errstr, strerror(errno), ERRMAX);
}

void
oserror(void)
{
	error(strerror(errno));
}

static Qid
fsqid(struct stat *st)
{
	Qid q;
	int dev;
	static int nqdev;
	static uchar *qdev;

	if(qdev == 0)
		qdev = smalloc(65536U);

	q.type = 0;
	if((st->st_mode&S_IFMT) ==  S_IFDIR)
		q.type = QTDIR;

	dev = st->st_dev & 0xFFFFUL;
	if(qdev[dev] == 0)
		qdev[dev] = ++nqdev;
	
	q.path = (vlong)qdev[dev]<<48;
	q.path ^= st->st_ino;
	q.vers = st->st_mtime;
	
	return q;
}

static Chan*
fsattach(char *spec)
{
	struct stat st;
	Chan *c;
	UnixFd *ufd;
	int plan9, dev;
	
	dev = 1;
	plan9 = 0;
	if(spec && spec[0]){
		if(strcmp(spec, "plan9") == 0) {
			plan9 = 1;
			dev = 2;
		} else{
			snprint(up->genbuf, sizeof up->genbuf, "no file system #%C%s", FsChar, spec);
			error(up->genbuf);
		}
	}

	if(plan9){
		if(localroot == nil)
			error("no #Zplan9 root without -r");
		if(stat(localroot, &st) < 0)
			oserror();
	}else{
		if(stat("/", &st) < 0)
			oserror();
	}

	c = devattach(FsChar, spec);
	ufd = mallocz(sizeof(UnixFd), 1);
	ufd->path = newpath("/");
	ufd->plan9 = plan9;
	ufd->fd = -1;

	c->aux = ufd;
	c->dev = dev;
	c->qid = fsqid(&st);
	
	if(Trace)
		print("fsattach /\n");

	return c;
}

static Chan*
fsclone(Chan *c, Chan *nc)
{
	UnixFd *ufd;
	
	ufd = mallocz(sizeof(UnixFd), 1);
	*ufd = *(UnixFd*)c->aux;
	if(ufd->path)
		incref(&ufd->path->ref);
	ufd->fd = -1;
	nc->aux = ufd;
	return nc;
}

static char*
lastelem(char *s)
{
	char *t;

	if(s[0] == '/' && s[1] == 0)
		return s;
	t = strrchr(s, '/');
	if(t == nil)
		return s;
	return t+1;
}

static char*
fspath(Chan *c, char *suffix)
{
	char *s, *t;
	int len;
	UnixFd *ufd;
	
	ufd = c->aux;
	s = ufd->path->s;
	if(ufd->plan9){
		len = strlen(localroot)+strlen(s)+1;
		if(suffix)
			len += 1+strlen(suffix);
		t = smalloc(len);
		strcpy(t, localroot);
		strcat(t, s);
	}else{
		len = strlen(s)+1;
		if(suffix)
			len += 1+strlen(suffix);
		t = smalloc(len);
		strcpy(t, s);
	}
	if(suffix){
		if(s[strlen(s)-1] != '/')
			strcat(t, "/");
		strcat(t, suffix);
	}
	return t;
}

static int
fswalk1(Chan *c, char *name)
{
	struct stat st;
	char *path;
	UnixFd *ufd;
	
	ufd = c->aux;
	if(strcmp(name, "..") == 0 && strcmp(ufd->path->s, "/") == 0)
		return 0;
	
	path = fspath(c, name);
	if(stat(path, &st) < 0){
		if(Trace)
			print("fswalk1 %s (%s)\n", path, strerror(errno));	
		free(path);
		return -1;
	}
	if(Trace)
		print("fswalk1 %s\n", path);	
	free(path);
	
	c->qid = fsqid(&st);
	return 0;
}

static void
replacepath(Chan *c, Path *p)
{
	UnixFd *ufd;
	
	ufd = c->aux;
	incref(&p->ref);
	pathclose(ufd->path);
	ufd->path = p;
}

static Walkqid*
fswalk(Chan *c, Chan *nc, char **name, int nname)
{
	int i;
	Path *path;
	Walkqid *wq;
	UnixFd *ufd;

	if(nc != nil)
		panic("fswalk: nc != nil");
	wq = smalloc(sizeof(Walkqid)+(nname-1)*sizeof(Qid));
	nc = devclone(c);
	fsclone(c, nc);
	ufd = c->aux;
	path = ufd->path;
	incref(&path->ref);

	wq->clone = nc;
	for(i=0; i<nname; i++){
		ufd = nc->aux;
		replacepath(nc, path);
		if(fswalk1(nc, name[i]) < 0){
			if(i == 0){
				pathclose(path);
				cclose(nc);
				free(wq);
				error(Enonexist);
			}
			break;
		}
		path = addelem(path, name[i], nil);
		wq->qid[i] = nc->qid;
	}
	replacepath(nc, path);
	pathclose(path);
	if(i != nname){
		cclose(nc);
		wq->clone = nil;
	}
	wq->nqid = i;
	return wq;
}

static int
fsdirstat(char *path, int dev, Dir *d)
{
	int fd;
	struct stat st;
	
	if(stat(path, &st) < 0 && lstat(path, &st) < 0)
		return -1;

	d->name = lastelem(path);
	d->uid = uidtoname(st.st_uid);
	d->gid = gidtoname(st.st_gid);
	d->muid = "";
	d->qid = fsqid(&st);
	d->mode = (d->qid.type<<24) | (st.st_mode&0777);
	d->atime = st.st_atime;
	d->mtime = st.st_mtime;
	d->length = st.st_size;
	if(S_ISBLK(st.st_mode) && (fd = open(path, O_RDONLY)) >= 0){
		d->length = disksize(fd, &st);
		close(fd);
	}
	
	// devmnt leaves 1-9 unused so that we can steal them.
	// it is easier for all involved if #Z shows M as the file type instead of Z.
	// dev is c->dev, either 1 (#Z) or 2 (#Zplan9).
	d->type = 'M';
	d->dev = dev;
	return 0;
}

static int
fsstat(Chan *c, uchar *buf, int n)
{
	Dir d;
	char *path;
	UnixFd *ufd;

	ufd = c->aux;
	if(Trace)
		print("fsstat %s\n", ufd->path->s);

	if(n < BIT16SZ)
		error(Eshortstat);

	path = fspath(c, nil);
	if(fsdirstat(path, c->dev, &d) < 0){
		free(path);
		oserror();
	}
	if(strcmp(ufd->path->s, "/") == 0)
		d.name = "/";
	n = convD2M(&d, buf, n);
	free(path);
	return n;
}

static int
opensocket(UnixFd *ufd, char *path)
{
	int fd;
	struct stat st;
	struct sockaddr_un su;
	
	if(stat(path, &st) < 0)
		return -1;
	if(!S_ISSOCK(st.st_mode))
		return -1;
	memset(&su, 0, sizeof su);
	su.sun_family = AF_UNIX;
	if(strlen(path)+1 > sizeof su.sun_path){
		errno = ENAMETOOLONG;
		return -1;
	}
	strcpy(su.sun_path, path);
	if((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
		return -1;
	if(connect(fd, (struct sockaddr*)&su, sizeof su) < 0){
		close(fd);
		return -1;
	}
	ufd->fd = fd;
	ufd->issocket = 1;
	return 0;
}		

static Chan*
fsopen(Chan *c, int mode)
{
	char *path;
	int m;
	UnixFd *ufd;
	
	ufd = c->aux;
	if(Trace)
		print("fsopen %s %#x\n", ufd->path->s, mode);

	if(mode & ~(OTRUNC|ORCLOSE|3))
		error(Ebadarg);

	if((c->qid.type & QTDIR) && mode != OREAD)
		error(Eperm);

	
	if((c->qid.type&QTDIR) && mode != OREAD)
		error(Eperm);

	c->mode = openmode(mode);
	path = fspath(c, nil);
	if(c->qid.type & QTDIR){
		ufd->dir = opendir(path);
		if(ufd->dir == nil){
			free(path);
			oserror();
		}
		ufd->diroffset = 0;
		ufd->nextde = nil;
	}else{
		m = mode & 3;
		if(m == OEXEC)
			m = OREAD;
		if(mode & OTRUNC)
			m |= O_TRUNC;
		if((ufd->fd = open(path, m)) < 0 && opensocket(ufd, path) < 0){
			free(path);
			oserror();
		}
	}
	free(path);
	c->flag |= COPEN;
	return c;
}

static void
fscreate(Chan *c, char *name, int mode, ulong perm)
{
	char *path, *path0;
	int fd, mm;
	UnixFd *ufd;
	struct stat st;

	ufd = c->aux;
	if(Trace)
		print("fscreate %s %#x %#o\n", ufd->path->s, mode, perm);

	if(!(c->qid.type & QTDIR))
		error(Enotdir);

	if(mode & ~(OTRUNC|ORCLOSE|3))
		error(Ebadarg);

	if(perm & ~(DMDIR|0777))
		error(Ebadarg);

	path0 = fspath(c, nil);
	path = fspath(c, name);
	if(waserror()){
		free(path);
		free(path0);
		nexterror();
	}
	
	if(stat(path0, &st) < 0)
		oserror();

	if(perm & DMDIR){
		if(mode != OREAD)
			error(Eperm);
		if(mkdir(path, perm & 0777) < 0)
			oserror();
		if((fd = open(path, 0)) < 0)
			oserror();
		// Be like Plan 9 file servers: inherit mode bits 
		// and group from parent.
		fchmod(fd, perm & st.st_mode & 0777);
		fchown(fd, -1, st.st_gid);
		if(fstat(fd, &st) < 0){
			close(fd);
			oserror();
		}
		close(fd);
		if((ufd->dir = opendir(path)) == nil)
			oserror();
		ufd->diroffset = 0;
		ufd->nextde = nil;
	}else{
		mm = mode & 3;
		if(mode & OTRUNC)
			mm |= O_TRUNC;
		if((fd = open(path, mm|O_CREAT, 0666)) < 0)
			oserror();
		// Be like Plan 9 file servers: inherit mode bits 
		// and group from parent.
		fchmod(fd, perm & st.st_mode & 0777);
		fchown(fd, -1, st.st_gid);
		if(fstat(fd, &st) < 0){
			close(fd);
			oserror();
		}
		ufd->fd = fd;
	}
	free(path);
	free(path0);
	poperror();
	
	ufd->path = addelem(ufd->path, name, nil);
	c->qid = fsqid(&st);
	c->offset = 0;
	c->flag |= COPEN;
	c->mode = openmode(mode);
}

static void
fsclose(Chan *c)
{
	UnixFd *ufd;
	char *path;
	
	ufd = c->aux;
	if(Trace)
		print("fsclose %s\n", ufd->path->s);

	if(c->flag & COPEN) {
		if(c->flag & CRCLOSE) {
			path = fspath(c, nil);
			unlink(path);
			free(path);
		}
		if(c->qid.type & QTDIR)
			closedir(ufd->dir);
		else
			close(ufd->fd);
	}
	if(ufd->path)
		pathclose(ufd->path);
	free(ufd);
}

static long fsdirread(Chan*, uchar*, long, vlong);

static long
fsread(Chan *c, void *va, long n, vlong offset)
{
	int r;
	UnixFd *ufd;

	if(c->qid.type & QTDIR)
		return fsdirread(c, va, n, offset);

	ufd = c->aux;
	if(ufd->issocket)
		r = read(ufd->fd, va, n);
	else
		r = pread(ufd->fd, va, n, offset);
	if(r < 0)
		oserror();
	return r;
}

static long
fswrite(Chan *c, void *va, long n, vlong offset)
{
	int r;
	UnixFd *ufd;

	ufd = c->aux;
	if(ufd->issocket)
		r = write(ufd->fd, va, n);
	else
		r = pwrite(ufd->fd, va, n, offset);
	if(r < 0)
		oserror();
	return r;
}

static int
fswstat(Chan *c, uchar *buf, int n)
{
	char *elem, *path, *npath, *strs, *t;
	int nn;
	Dir d;
	UnixFd *ufd;

	if(n < 2)
		error(Ebadstat);

	nn = GBIT16((uchar*)buf);
	strs = smalloc(nn);
	if(convM2D(buf, n, &d, strs) != n){
		free(strs);
		error(Ebadstat);
	}
	
	path = fspath(c, nil);
	if(waserror()){
		free(path);
		free(strs);
		nexterror();
	}
	
	if(d.muid[0])
		error("cannot change muid");

	if(d.uid[0] || d.gid[0]){
		int uid, gid;
		
		uid = -1;
		gid = -1;
		if(d.uid[0] && (uid = nametouid(d.uid)) < 0)
			error("unknown uid");
		if(d.gid[0] && (gid = nametogid(d.gid)) < 0)
			error("unknown gid");
		if(chown(path, uid, gid) < 0)
			oserror();
	}
	
	ufd = c->aux;
	elem = lastelem(path);
	if(d.name[0] && strcmp(d.name, elem) != 0){
		if(strchr(d.name, '/'))
			error(Ebadarg);
		npath = smalloc(strlen(path)+strlen(d.name)+1);
		strcpy(npath, path);
		t = strrchr(npath, '/');
		strcpy(t+1, d.name);
		if(rename(path, npath) < 0){
			free(npath);
			oserror();
		}
		free(npath);
	}
	
	if(~d.mode != 0 && chmod(path, d.mode&0777) < 0)
		oserror();

	// TODO: Code to change uid, gid.
	
	poperror();
	return n;
}

static int
isdots(char *name)
{
	if(name[0] != '.')
		return 0;
	if(name[1] == '\0')
		return 1;
	if(name[1] != '.')
		return 0;
	if(name[2] == '\0')
		return 1;
	return 0;
}

static long
fsdirread(Chan *c, uchar *va, long count, vlong offset)
{
	char *path;
	int n, total;
	struct dirent *de;
	UnixFd *ufd;
	Dir d;

	ufd = c->aux;
	qlock(&ufd->dirlock);
	if(waserror()){
		qunlock(&ufd->dirlock);
		nexterror();
	}

	if(ufd->diroffset != offset){
		if(offset != 0)
			error(Ebadarg);
		ufd->diroffset = 0;
		ufd->nextde = nil;
		rewinddir(ufd->dir);
	}

	total = 0;
	while(total+BIT16SZ < count) {
		if(ufd->nextde){
			de = ufd->nextde;
			ufd->nextde = nil;
		}
		else if((de = readdir(ufd->dir)) == nil)
			break;
		if(isdots(de->d_name))
			continue;
		path = fspath(c, de->d_name);
		if(fsdirstat(path, c->dev, &d) < 0){
			free(path);
			continue;
		}
		n = convD2M(&d, (uchar*)va+total, count-total);
		free(path);
		if(n == BIT16SZ){
			ufd->nextde = de;
			break;
		}
		total += n;
	}
	ufd->diroffset += total;
	qunlock(&ufd->dirlock);
	poperror();
	return total;
}

static void
fsremove(Chan *c)
{
	char *path;
	UnixFd *ufd;

	ufd = c->aux;
	if(Trace)
		print("fsremove %s\n", ufd->path->s);

	path = fspath(c, nil);
	if(waserror()){
		free(path);
		nexterror();
	}
	if(c->qid.type & QTDIR){
		if(rmdir(path) < 0)
			oserror();
	}else{
		if(remove(path) < 0)
			oserror();
	}
	free(path);
	poperror();
}

Dev fsdevtab = {
	FsChar,
	"fs",

	devreset,
	devinit,
	devshutdown,
	fsattach,
	fswalk,
	fsstat,
	fsopen,
	fscreate,
	fsclose,
	fsread,
	devbread,
	fswrite,
	devbwrite,
	fsremove,
	fswstat,
};


/* Uid management code adapted from u9fs */

/*
 * we keep a table by numeric id.  by name lookups happen infrequently
 * while by-number lookups happen once for every directory entry read
 * and every stat request.
 */
typedef struct User User;
struct User {
	int id;
	gid_t defaultgid;
	char *name;
	User *next;
};


static User *utab[64];
static User *gtab[64];

static User*
adduser(struct passwd *p)
{
	User *u;

	u = smalloc(sizeof(*u));
	u->id = p->pw_uid;
	kstrdup(&u->name, p->pw_name);
	u->next = utab[p->pw_uid%nelem(utab)];
	u->defaultgid = p->pw_gid;
	utab[p->pw_uid%nelem(utab)] = u;
	return u;
}

static User*
addgroup(struct group *g)
{
	User *u;

	u = smalloc(sizeof(*u));
	u->id = g->gr_gid;
	kstrdup(&u->name, g->gr_name);
	u->next = gtab[g->gr_gid%nelem(gtab)];
	gtab[g->gr_gid%nelem(gtab)] = u;
	return u;
}

static User*
uname2user(char *name)
{
	int i;
	User *u;
	struct passwd *p;

	for(i=0; i<nelem(utab); i++)
		for(u=utab[i]; u; u=u->next)
			if(strcmp(u->name, name) == 0)
				return u;

	if((p = getpwnam(name)) == nil)
		return nil;
	return adduser(p);
}

static User*
uid2user(int id)
{
	User *u;
	struct passwd *p;

	for(u=utab[id%nelem(utab)]; u; u=u->next)
		if(u->id == id)
			return u;

	if((p = getpwuid(id)) == nil)
		return nil;
	return adduser(p);
}

static User*
gname2user(char *name)
{
	int i;
	User *u;
	struct group *g;

	for(i=0; i<nelem(gtab); i++)
		for(u=gtab[i]; u; u=u->next)
			if(strcmp(u->name, name) == 0)
				return u;

	if((g = getgrnam(name)) == nil)
		return nil;
	return addgroup(g);
}

static User*
gid2user(int id)
{
	User *u;
	struct group *g;

	for(u=gtab[id%nelem(gtab)]; u; u=u->next)
		if(u->id == id)
			return u;

	if((g = getgrgid(id)) == nil)
		return nil;
	return addgroup(g);
}

static char*
uidtoname(int uid)
{
	User *u;
	
	u = uid2user(uid);
	if(u == nil)
		return "?";
	return u->name;
}

static char*
gidtoname(int gid)
{
	User *u;
	
	u = gid2user(gid);
	if(u == nil)
		return "?";
	return u->name;
}

static int
nametouid(char *name)
{
	User *u;
	
	u = uname2user(name);
	if(u == nil)
		return -1;
	return u->id;
}

static int
nametogid(char *name)
{
	User *u;
	
	u = gname2user(name);
	if(u == nil)
		return -1;
	return u->id;
}

#if defined(__linux__)

static vlong
disksize(int fd, struct stat *st)
{
	uvlong u64;
	long l;
	struct hd_geometry geo;
	
	memset(&geo, 0, sizeof geo);
	l = 0;
	u64 = 0;
#ifdef BLKGETSIZE64
	if(ioctl(fd, BLKGETSIZE64, &u64) >= 0)
		return u64;
#endif
	if(ioctl(fd, BLKGETSIZE, &l) >= 0)
		return l*512;
	if(ioctl(fd, HDIO_GETGEO, &geo) >= 0)
		return (vlong)geo.heads*geo.sectors*geo.cylinders*512;
	return 0;
}

#elif defined(__FreeBSD__) && defined(DIOCGMEDIASIZE)

static vlong
disksize(int fd, struct stat *st)
{
	off_t mediasize;
	
	if(ioctl(fd, DIOCGMEDIASIZE, &mediasize) >= 0)
		return mediasize;
	return 0;
}

#elif defined(__APPLE__)

static vlong
disksize(int fd, struct stat *st)
{
	uvlong bc;
	unsigned int bs;

	bs = 0;
	bc = 0;
	ioctl(fd, DKIOCGETBLOCKSIZE, &bs);
	ioctl(fd, DKIOCGETBLOCKCOUNT, &bc);
	if(bs >0 && bc > 0)
		return bc*bs;
	return 0;
}

#else

static vlong
disksize(int fd, struct stat *st)
{
	return 0;
}

#endif
