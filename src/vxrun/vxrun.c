#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <setjmp.h>
#include "vx32.h"
#include "args.h"
#include "libvxc_dat.h"

#define syscall xxxsyscall // FIXME
#include "libvxc/syscall.h"

const char *argv0;

extern int vx_elfbigmem;

static const char *progname;
static pid_t progpid;

static int verbose = 0;

static int runreps = 1;
static jmp_buf exitbuf;

static void fatal(const char *fmt, ...)
{
	va_list ap;
	fprintf(stderr, "%s: fatal error: ", progname);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	exit(2);
}

static void dumpregs(struct vxproc *p)
{
	struct vxcpu *c = p->cpu;

	fprintf(stderr, "eax %08x  ecx %08x  edx %08x  ebx %08x\n",
		c->reg[EAX], c->reg[ECX], c->reg[EDX], c->reg[EBX]);
	fprintf(stderr, "esp %08x  ebp %08x  esi %08x  edi %08x\n",
		c->reg[ESP], c->reg[EBP], c->reg[ESI], c->reg[EDI]);
	fprintf(stderr, "eip %08x  eflags %08x\n",
		c->eip, c->eflags);

//	for (int i = 0; i < 8; i++) {
//		int32_t *val = r.xmm[i].i32;
//		fprintf(stderr, "xmm%d %08x%08x%08x%08x\n",
//			i, val[3], val[2], val[1], val[0]);
//	}
}

static uint32_t mode2vxc(uint32_t st)
{
	uint32_t vxc = st & 0x0777;  // At least we agree on this!
	if (st & S_ISVTX)
		vxc |= VXC_S_ISVTX;
	if (st & S_ISGID)
		vxc |= VXC_S_ISGID;
	if (st & S_ISUID)
		vxc |= VXC_S_ISUID;
	switch (st & S_IFMT) {
	case S_IFREG:
		vxc |= VXC_S_IFREG;
		break;
	case S_IFDIR:
		vxc |= VXC_S_IFDIR;
		break;
	case S_IFLNK:
		vxc |= VXC_S_IFLNK;
		break;
	case S_IFSOCK:
		vxc |= VXC_S_IFSOCK;
		break;
	case S_IFIFO:
		vxc |= VXC_S_IFIFO;
		break;
	case S_IFBLK:
		vxc |= VXC_S_IFBLK;
		break;
	default:
	case S_IFCHR:
		vxc |= VXC_S_IFCHR;
		break;
	}
	return vxc;
}

static void stat2vxc(struct vxc_stat *vst, struct stat *st)
{
	vst->dev = st->st_dev;
	vst->ino = st->st_ino;
	vst->mode = mode2vxc(st->st_mode);
	vst->nlink = st->st_nlink;
	vst->uid = st->st_uid;
	vst->gid = st->st_gid;
	vst->rdev = st->st_rdev;
	vst->blksize = st->st_blksize;
	vst->blocks = st->st_blocks;
	vst->size = st->st_size;
	vst->atime = st->st_atime;
	vst->mtime = st->st_mtime;
	vst->ctime = st->st_ctime;
}

static int checkstring(vxmem *mem, char *base, uint32_t addr)
{
	uint32_t eaddr;

	for (;;) {
		if (!vxmem_checkperm(mem, addr, 1, VXPERM_READ, NULL))
			return 0;
		eaddr = (addr + 4096) & ~(4096-1);
		if (memchr(base + addr, 0, eaddr - addr))
			return 1;
		addr = eaddr;
	}
}

static int doexec(vxproc*, char*, uint32_t, uint32_t, uint32_t);

int trace;

#define RET proc->cpu->reg[EAX]
#define NUM proc->cpu->reg[EAX]
#define ARG1 proc->cpu->reg[EDX]
#define ARG2 proc->cpu->reg[ECX]
#define ARG3 proc->cpu->reg[EBX]
#define ARG4 proc->cpu->reg[EDI]
#define ARG5 proc->cpu->reg[ESI]

static void dosyscall(vxproc *proc)
{
	int fd, p[2], *vp, ret, mode, umode;
	uint32_t addr, saddr, oaddr;
	int len;
	vxmmap *m;
	struct stat st;
	uint32_t inc;
	uint32_t secs;
	
	m = vxmem_map(proc->mem, 0);

	switch (NUM) {
	case VXSYSEXIT:
		if (ARG1 != 0 || runreps == 1)
			exit(ARG1);
		longjmp(exitbuf, 1);	// back for more repetitions...
	
	case VXSYSBRK:
		addr = ARG1;
		inc = 1<<20;
		addr = (addr + inc - 1) & ~(inc - 1);
		oaddr = m->size;
		if (addr == oaddr) {
			ret = 0;
			break;
		}
		ret = 0;
		if (addr > m->size)
			ret = vxmem_resize(proc->mem, addr);
		if (trace)
			fprintf(stderr, "sbrk %p -> %p / %p; %d\n", oaddr, addr, ARG1, ret);
		if (ret < 0)
			fprintf(stderr, "warning: sbrk failed. caller will be unhappy!\n");
		if (ret >= 0) {
			if (addr > oaddr)
				ret = vxmem_setperm(proc->mem, oaddr, addr - oaddr, VXPERM_READ|VXPERM_WRITE);
			if(ret < 0)
				fprintf(stderr, "setperm is failing! %lx + %lx > %lx ? \n", oaddr, addr - oaddr, m->size);
		}
		break;

	case VXSYSREAD:
		fd = ARG1;
		addr = ARG2;
		len = ARG3;
		if (!vxmem_checkperm(proc->mem, addr, len, VXPERM_WRITE, NULL))
			fatal("bad arguments to read");
		ret = read(fd, (char*)m->base + addr, len);
		break;
	
	case VXSYSWRITE:
		fd = ARG1;
		addr = ARG2;
		len = ARG3;
		if (!vxmem_checkperm(proc->mem, addr, len, VXPERM_READ, NULL))
			fatal("bad arguments to write");
		ret = write(fd, (char*)m->base + addr, len);
		break;
	
	case VXSYSSTAT:
		addr = ARG1;
		saddr = ARG2;
		if (!checkstring(proc->mem, m->base, addr) ||
		    !vxmem_checkperm(proc->mem, saddr, sizeof(struct vxc_stat), VXPERM_WRITE, NULL)){
		einval:
			RET = -EINVAL;
			goto out;
		}
		if ((ret = stat((char*)m->base + addr, &st)) >= 0)
			stat2vxc((struct vxc_stat*)((char*)m->base + saddr), &st);
		if (trace)
			fprintf(stderr, "stat %x/%s => %d\n", addr, (char*)m->base+addr, ret);
		break;

	case VXSYSFSTAT:
		fd = ARG1;
		saddr = ARG2;
		if (!vxmem_checkperm(proc->mem, saddr, sizeof(struct vxc_stat), VXPERM_WRITE, NULL)){
			RET = -EINVAL;
			goto out;
		}
		if ((ret = fstat(fd, &st)) >= 0)
			stat2vxc((struct vxc_stat*)((char*)m->base + saddr), &st);
		if (trace)
			fprintf(stderr, "fstat %d => %d\n", fd, ret);
		break;

	case VXSYSREMOVE:
		addr = ARG1;
		if (!checkstring(proc->mem, m->base, addr))
			goto einval;
		char *name = (char*)m->base+addr;
		if (stat(name, &st) >= 0 && S_ISDIR(st.st_mode))
			ret = rmdir(name);
		else
			ret = unlink(name);
		break;
		
	case VXSYSOPEN:
		addr = ARG1;
		mode = ARG2;
		umode = mode&3;
		if(mode & VXC_O_CREAT)
			umode |= O_CREAT;
		if(mode & VXC_O_EXCL)
			umode |= O_EXCL;
		if(mode & VXC_O_NOCTTY)
			umode |= O_NOCTTY;
		if(mode & VXC_O_TRUNC)
			umode |= O_TRUNC;
		if(mode & VXC_O_APPEND)
			umode |= O_APPEND;
		if(mode & VXC_O_NONBLOCK)
			umode |= O_NONBLOCK;
		if(mode & VXC_O_SYNC)
			umode |= O_SYNC;
		if (!checkstring(proc->mem, m->base, addr))
			goto einval;
		ret = open((char*)m->base+addr, umode, ARG3);
		if(trace)
			fprintf(stderr, "open %s %#x %#o => %d\n", (char*)m->base+addr, ARG2, ARG3, ret);
		break;

	case VXSYSMKDIR:
		addr = ARG1;
		if (!checkstring(proc->mem, m->base, addr))
			goto einval;
		ret = mkdir((char*)m->base+addr, ARG2);
		if (trace)
			fprintf(stderr, "mkdir %s %#o => %d\n", (char*)m->base+addr, ARG2, ret);
		break;

	case VXSYSCLOSE:
		fd = ARG1;
		if (fd < 0)
			goto einval;
		ret = close(fd);
		break;
	
	case VXSYSLSEEK:
		ret = lseek(ARG1, (int32_t)ARG2, ARG3);
		break;

	case VXSYSTIME:
		addr = ARG1;
		if (!vxmem_checkperm(proc->mem, addr, sizeof(struct vxc_timeval), VXPERM_WRITE, NULL))
			goto einval;
		struct timeval tv;
		gettimeofday(&tv, NULL);
		struct vxc_timeval *vtv = (void*)((char*)m->base + addr);
		vtv->tv_sec = tv.tv_sec;
		vtv->tv_usec = tv.tv_usec;
		ret = 0;
		break;

	case VXSYSFCNTL:
		// TODO: check better
		ret = fcntl(ARG1, ARG2, ARG3);
		if(trace)
			fprintf(stderr, "fcntl %d %d %d => %d\n", ARG1, ARG2, ARG3, ret);
		break;
	
	case VXSYSDUP:
		if(ARG2 == -1)
			ret = dup(ARG1);
		else
			ret = dup2(ARG1, ARG2);
		break;

	case VXSYSFORK:
		vxmem_unmap(proc->mem, m);
		vxmem_unmap(proc->mem, proc->mem->mapped);
		proc->mem->mapped = NULL;

		vxmem *nmem = vxmem_chunk_copy(proc->mem);
		if (nmem == NULL) {
			RET = -errno;
			return;
		}
		ret = fork();
		if (ret < 0)
			ret = -errno;
		if (ret == 0) {
			vxmem_free(proc->mem);
			proc->mem = nmem;
		} else {
			vxmem_free(nmem);
		}
		RET = ret;
		return;
	
	case VXSYSWAITPID:
		addr = ARG2;
		if (addr && !vxmem_checkperm(proc->mem, addr, 4, VXPERM_WRITE, NULL))
			goto einval;
		ret = waitpid(ARG1, addr ? (int*)((char*)m->base+addr) : 0, ARG3);
		break;

	case VXSYSEXEC:
		ret = doexec(proc, m->base, ARG1, ARG2, ARG3);
		break;

	case VXSYSPIPE:
		addr = ARG1;
		if (!vxmem_checkperm(proc->mem, addr, 8, VXPERM_WRITE, NULL))
			goto einval;
		ret = pipe(p);
		if (ret >= 0) {
			vp = (int*)((char*)m->base + addr);
			vp[0] = p[0];
			vp[1] = p[1];
		}
		break;
	
	case VXSYSSLEEP:
		secs = ARG1;
		if(trace)
			fprintf(stderr, "sleep %d\n", secs);
		if (secs == 0)
			ret = 0;
		else
			ret = sleep(secs);
		break;

	// Just to provide the classic "null system call" test...
	case VXSYSGETPID:
		fprintf(stderr, "getpid\n");
		ret = progpid;
		break;

	default:
		dumpregs(proc);
		fatal("vxrun: bad system call %d", NUM);
		ret = -1;
	}
	
	if (ret < 0)
		ret = -errno;
	RET = ret;
out:
	//vxmem_unmap(proc->mem, m);	 XXX get rid of ref count?
	;
}

static char**
convertargs(vxproc *proc, char *base, uint32_t args)
{
	int n;
	uint32_t a, s;
	char **argv;

	if (args == 0)
		return NULL;
	for (a=args;; a+=4) {
		if (!vxmem_checkperm(proc->mem, a, 4, VXPERM_READ, NULL)){
			if(trace)
				fprintf(stderr, "bad args addr %p\n", a);
			return NULL;
		}
		s = *(uint32_t*)(base+a);
		if (s == 0)
			break;
		if (!checkstring(proc->mem, base, s)){
			if(trace)
				fprintf(stderr, "bad arg string %p\n", s);
			return NULL;
		}
	}
	n = (a - args) / 4 + 1;
	argv = malloc(n*sizeof argv[0]);
	if (argv == NULL)
		return NULL;
	n = 0;
	for (a=args;; a+=4) {
		s = *(uint32_t*)(base+a);
		if (s == 0)
			break;
		argv[n++] = base+s;
	}
	argv[n] = NULL;
	return argv;
}

static int
doexec(vxproc *proc, char *base, uint32_t exe, uint32_t args, uint32_t envs)
{
	int i;

	if(!checkstring(proc->mem, base, exe)){
		if(trace)
			fprintf(stderr, "exec [bad string %p]\n", exe);
	einval:
		errno = EINVAL;
		return -1;
	}
	if(trace)
		fprintf(stderr, "exec %s\n", base+exe);
	char **argv = convertargs(proc, base, args);
	if(args && !argv)
		goto einval;
	char **envv = convertargs(proc, base, envs);
	if(envs && !envv){
		free(argv);
		goto einval;
	}
	if(trace){
		fprintf(stderr, "exec %s", base+exe);
		if(argv)
			for(i=0; argv[i]; i++)
				fprintf(stderr, " %s", argv[i]);
		fprintf(stderr, "\n");
	}
	execve(base+exe, argv, envv);
	free(argv);
	free(envv);
	return -1;
}

extern char **environ;

void runprog(const char *const *argv)
{
	vxproc *volatile p = vxproc_alloc();
	if (p == NULL)
		fatal("vxproc_new: %s\n", strerror(errno));
	p->allowfp = 1;

	const char *loadname = argv[0];
	if (vxproc_loadelffile(p, loadname, &argv[0],
			(const char**)environ) < 0)
		fatal("vxproc_loadelffile: %s\n", strerror(errno));

	// Come back here and return if the guest process calls exit(0)
	// and we still have more repetitions to run.
	if (setjmp(exitbuf)) {
		vxproc_free(p);
		return;
	}

	// Simple execution loop.
	for (;;) {
		int rc = vxproc_run(p);
		if (rc < 0)
			fatal("vxproc_run: %s\n", strerror(errno));
		if (rc == VXTRAP_SYSCALL) {
			dosyscall(p);
			continue;
		}
		dumpregs(p);
		fatal("vxproc_run trap %#x\n", rc);
	}
}

int main(int argc, const char *const *argv)
{
	int i;
	progname = argv[0];
	progpid = getpid();
	int reps = 1;

	vx_elfbigmem = 1;

	ARGBEGIN{
	case 't':
		trace++;
		break;
	case 'd':
		vx32_debugxlate++;
		break;
	case 'v':
		verbose++;
		break;
	case 'r':
		runreps = atoi(ARGF());
		if (runreps < 1) {
			fprintf(stderr, "Invalid repeat count %d\n", runreps);
			exit(1);
		}
		break;
	default:
	usage:
		fprintf(stderr, "Usage: %s [-dtv] <vx-program> <args>\n",
			progname);
		exit(1);
	}ARGEND
	
	if (argc < 1)
		goto usage;

	FILE *f = fopen("/dev/tty", "w");
	if(f && verbose){
		char buf[1000];
		if(getcwd(buf, sizeof buf) != NULL)
			fprintf(f, "cd %s\n", buf);
		fprintf(f, "vxrun");
		for(i=0; i<argc; i++)
			fprintf(f, " %s", argv[i]);
		fprintf(f, "\n");
	}

	vx32_siginit();

	// Repeatedly load, execute, and unload the guest process
	// as many times as requested.
	do {
		//fprintf(stderr, "run, reps=%d\n", runreps);
		runprog(argv);
	} while (--runreps > 0);

	return 0;	// not reached
}

