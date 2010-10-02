// ELF program loader

#define _XOPEN_SOURCE 500

#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "vx32.h"
#include "vx32impl.h"
#include "elf.h"
#include "words.h"

#define VX32_ARG_MAX 10*1024
#define VX32_STACK 64*1024
#define VXMEMSIZE (1<<30)

int vx_elfbigmem;

static int elfloader(vxproc *p,
	ssize_t (*readcb)(void*, off_t, void*, size_t), void*,
	const char *const *argv, const char *const *envp);

// From a file.

struct filedesc
{
	int fd;
};

static ssize_t loadfilecb(void *cbdata, off_t offset, void *buf, size_t size)
{
	ssize_t rc;
	struct filedesc *desc;
	
	desc = cbdata;
	rc = pread(desc->fd, buf, size, offset);
	return rc;
}

int vxproc_loadelffile(vxproc *p, const char *file,
	const char *const *argv, const char *const *envp)
{
	int fd, rc;
	struct filedesc desc;
	
	if ((fd = open(file, O_RDONLY)) < 0)
		return -1;
	desc.fd = fd;
	rc = elfloader(p, loadfilecb, &desc, argv, envp);
	close(fd);
	return rc;
}

// From memory.

struct memdesc
{
	const void *buf;
	size_t size;
};

static ssize_t loadmemcb(void *cbdata, off_t offset, void *buf, size_t size)
{
	struct memdesc *desc;
	
	desc = cbdata;
	if (offset >= desc->size || offset + size >= desc->size)
		return 0;
	memmove(buf, desc->buf + offset, size);
	return size;
}

int vxproc_loadelfmem(vxproc *p, const void *exe, size_t size,
	const char *const *argv, const char *const *envp)
{
	struct memdesc desc;
	
	desc.buf = exe;
	desc.size = size;
	return elfloader(p, loadmemcb, &desc, argv, envp);
}


// In general.

// Count number of args in array.
static int countargs(const char *const *argv)
{
	int i;
	
	for(i=0; argv[i]; i++)
		;
	return i;
}

// Copy the strings from argv onto the stack, recording
// their guest address in gargv.
static int copystrings(uint8_t *base, uint32_t *espp, int argc, const char *const argv[], uint32_t *gargv)
{
	int i;
	uint32_t esp = *espp;
	for (i = argc-1; i >= 0; i--) {
		int len = strlen(argv[i]);
		if (len + 4096 > esp)
			return -1;
		esp -= len+1;
		memmove(base+esp, argv[i], len+1);
		gargv[i] = esp;
	}
	*espp = esp;
	return 0;
}

// Copy pre-translated pointer array into guest address space
static int copyptrs(uint8_t *base, uint32_t *espp, int argc, uint32_t *gargv)
{
	uint32_t esp = *espp;
	esp &= ~3;  // align
	if (argc * 4 + 4096 > esp)
		return -1;
	esp -= argc*4;
	memmove(base+esp, gargv, argc*4);
	*espp = esp;
	return 0;
}

#define ELF_MAX_PH 32

static int elfloader(vxproc *proc,
              ssize_t (*readcb)(void*, off_t, void*, size_t),
              void *cbdata,
              const char *const *argv, const char *const *envp)
{
	vxmem *mem;
	int i;
	size_t size;
	ssize_t act;
	struct Proghdr ph[ELF_MAX_PH];
	vxmmap *mm;
	static const char *null;

	if (argv == NULL)
		argv = &null;
	if (envp == NULL)
		envp = &null;

	size = 4096;
	// For "big mem" we need more than 1/2 GB, but 64-bit Linux
	// has only about 1 GB of address space to give out with MAP_32BIT,
	// and we've used up some of it for the vxemu structure.  
	// Used to ask for (1<<30) - (1<<24), which should be close enough to 1GB
	// to run the SPEC programs but leave enough for things like vxemu.
	// On Ubuntu 8.10, I get intermittent ouf of memory errors from this
	// mmap, so back off to 1<<29.
	if (vx_elfbigmem)
		size = (1<<29);

	mm = NULL;

	if ((mem = vxmem_chunk_new(size)) == NULL)
		return -1;

	// Read and check the ELF header
	struct Elf32 h;
	act = readcb(cbdata, 0, &h, sizeof(h));
	if (act < 0)
		goto error;
	if (act < sizeof(h)) {
	noexec:
		errno = ENOEXEC;
		goto error;
	}
	if (ltoh32(h.e_magic) != ELF_MAGIC)
		goto noexec;

	// Read the program header table
	off_t phoff = ltoh32(h.e_phoff);
	size_t phnum = ltoh16(h.e_phnum);
	if (phnum <= 0 || phnum > ELF_MAX_PH)	// arbitrary limit for security
		goto noexec;

	act = readcb(cbdata, phoff, ph, phnum * sizeof ph[0]);
	if (act < 0)
		goto error;
	if (act < phnum * sizeof ph[0])
		goto noexec;
	
	// Load each program segment
	size_t stackhi = VXMEMSIZE;
	size_t addrhi = 0;
	for (i = 0; i < phnum; i++) {
		const struct Proghdr *p = &ph[i];
		if (ltoh32(p->p_type) != ELF_PROG_LOAD)
			continue;

		// Validate the program segment
		off_t offset = ltoh32(p->p_offset);
		size_t filesz = ltoh32(p->p_filesz);
		size_t va = ltoh32(p->p_va);
		size_t memsz = ltoh32(p->p_memsz);
		if (filesz > memsz)
			goto noexec;

		// Validate the memory page region the segment loads into
		size_t memlo = VXPAGETRUNC(va);
		size_t memhi = VXPAGEROUND(va + memsz);
		if (memlo > VXMEMSIZE || memhi > VXMEMSIZE || memhi < memlo)
			goto noexec;

		// Make sure the VX process is big enough, and mapped
		if (size < memhi) {
			if (mm) {
				vxmem_unmap(mem, mm);
				mm = NULL;
			}
			if (vxmem_resize(mem, memhi) < 0)
				goto error;
			size = memhi;
		}

		if (mm == NULL) {
			mm = vxmem_map(mem, 0);
			if (mm == NULL)
				goto error;
		}

		// Temporarily give ourselves write permissions
		// on the segment in order to load it.
		if (vxmem_setperm(mem, memlo, memhi - memlo,
				VXPERM_READ | VXPERM_WRITE) < 0)
			return -1;

		// Load the segment.
		// Any bss portion is already cleared automatically
		// by virtue of the vxproc_clear() above.
		act = readcb(cbdata, offset, mm->base + va, filesz);
		if (act < 0)
			return -1;
		if (act < filesz)
			goto noexec;

		// Set permissions appropriately on the segment's pages
		int flags = ltoh32(p->p_flags);
		int perm = 0;
		switch (flags & (ELF_PROG_FLAG_READ | ELF_PROG_FLAG_WRITE |
				ELF_PROG_FLAG_EXEC)) {
		case ELF_PROG_FLAG_READ:
			perm = VXPERM_READ;
			break;
		case ELF_PROG_FLAG_READ | ELF_PROG_FLAG_WRITE:
			perm = VXPERM_READ | VXPERM_WRITE;
			break;
		case ELF_PROG_FLAG_READ | ELF_PROG_FLAG_EXEC:
			perm = VXPERM_READ | VXPERM_EXEC;
			break;
		default:
			goto noexec;	// invalid perms
		}
		if (vxmem_setperm(mem, memlo, memhi - memlo, perm) < 0)
			goto error;

		// Find the lowest-used va, for locating the stack
		if (va < stackhi)
			stackhi = va;

		// Find the highest va too
		if (memhi > addrhi)
			addrhi = memhi;
	}

	if (size > addrhi)
		vxmem_setperm(mem, addrhi, size - addrhi, VXPERM_READ|VXPERM_WRITE);

	if (mm == NULL) {
		mm = vxmem_map(mem, 0);
		if (mm == NULL)
			goto error;
	}

	// Set up the process's stack,
	// growing downward from the executable's base load address.
	// Initially we enable read/write access on 64K of stack,
	// but the process is free to change that if it wants a bigger stack.
	stackhi = VXPAGETRUNC(stackhi);
	if (stackhi < VX32_STACK)
		goto noexec;
	if (vxmem_setperm(mem, stackhi - VX32_STACK, VX32_STACK,
			VXPERM_READ | VXPERM_WRITE) < 0)
		goto error;
	proc->cpu->reg[ESP] = stackhi;

	// Push the argument and environment arrays on the stack.
	uint32_t esp = stackhi;
	uint32_t argc;
	uint32_t envc;
	argc = countargs(argv);
	envc = countargs(envp);
	uint32_t *argvenv = malloc((argc+1+envc+1)*sizeof argvenv[0]);
	if (argv == NULL)
		goto error;
	if (copystrings(mm->base, &esp, envc, envp, argvenv+argc+1) < 0 ||
	    copystrings(mm->base, &esp, argc, argv, argvenv) < 0) {
		free(argvenv);
		goto error;
	}
	if (copyptrs(mm->base, &esp, argc+1+envc+1, argvenv) < 0) {
		free(argvenv);
		goto error;
	}
	
	// Set up stack just like Linux: argc, then argv pointers begin, then env pointers.
	esp -= 4;
	*(uint32_t*)(mm->base+esp) = argc;

	if (proc->mem)
		vxmem_free(proc->mem);
	proc->mem = mem;
	
	// Set up the process's initial register state
	for (i = 0; i < 8; i++)
		proc->cpu->reg[i] = 0;
	proc->cpu->reg[ESP] = esp;
	proc->cpu->eflags = 0;
	proc->cpu->eip = ltoh32(h.e_entry);

	return 0;

error:
	if (mm)
		vxmem_unmap(mem, mm);
	vxmem_free(mem);
	return -1;
}

