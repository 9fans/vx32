#define _XOPEN_SOURCE 500
#define _GNU_SOURCE // for MAP_32BIT

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <errno.h>
#include <assert.h>

#include "vx32.h"
#include "vx32impl.h"

extern int vx_elfbigmem;

typedef struct vxmem_chunk vxmem_chunk;

struct vxmem_chunk {
	vxmem mem;
	int fd;
	off_t size;
	uint8_t *perm;
};

static void chunk_free(vxmem *mem)
{
	vxmem_chunk *chunk = (vxmem_chunk*)mem;

	if(mem->mapped){
		vxmem_unmap(mem, mem->mapped);
		mem->mapped = NULL;
	}

	free(chunk->perm);
	close(chunk->fd);
	free(chunk);
}

static int chunk_resize(vxmem *mem, size_t size)
{
	vxmem_chunk *chunk = (vxmem_chunk*)mem;
	uint8_t *perm;
	uint32_t onpage, npage;

	if(size == chunk->size)
		return 0;

	if(mem->mapped){
		assert(!vx_elfbigmem);
		vxmem_unmap(mem, mem->mapped);
		mem->mapped = NULL;
	}

	if(ftruncate(chunk->fd, size) < 0)
		return -1;
	onpage = VXPAGEROUND(chunk->size) / VXPAGESIZE;
	npage = VXPAGEROUND(size) / VXPAGESIZE;
	perm = realloc(chunk->perm, npage);
	if (perm == NULL)
		return -1;
	if(npage > onpage)
		memset(perm + onpage, 0, npage-onpage);
	chunk->perm = perm;
	chunk->size = size;
	return 0;
}	

vxmem *vxmem_chunk_copy(vxmem *mem)
{
	vxmem_chunk *chunk = (vxmem_chunk*)mem;
	uint8_t *perm;
	uint32_t i, npage;

	assert(mem->free == chunk_free);
	vxmem *nmem = vxmem_chunk_new(chunk->size);
	if (nmem == NULL)
		return NULL;
	npage = VXPAGEROUND(chunk->size) / VXPAGESIZE;
	int n = 0;
	vxmem_chunk *nchunk = (vxmem_chunk*)nmem;
	char *buf = malloc(4096);
	memmove(nchunk->perm, chunk->perm, npage);
	for (i=0; i<npage; i++) {
		if (nchunk->perm[i]) {
			vxmem_read(mem, buf, i*VXPAGESIZE, VXPAGESIZE);
			vxmem_write(nmem, buf, i*VXPAGESIZE, VXPAGESIZE);
			n++;
		}
	}
	free(buf);
	return nmem;
}

static ssize_t chunk_read(vxmem *mem, void *data, uint32_t addr, uint32_t len)
{
	vxmem_chunk *chunk = (vxmem_chunk*)mem;
	
	if (addr >= chunk->size)
		return -1;
	if (len > chunk->size - addr)
		len = chunk->size - addr;
	return pread(chunk->fd, data, len, addr);
}

static ssize_t chunk_write(vxmem *mem, const void *data, uint32_t addr, uint32_t len)
{
	vxmem_chunk *chunk = (vxmem_chunk*)mem;
	
	if (addr >= chunk->size)
		return -1;
	if (len > chunk->size - addr)
		len = chunk->size - addr;
	return pwrite(chunk->fd, data, len, addr);
}

static int chunk_checkperm(vxmem *mem, uint32_t addr, uint32_t len, uint32_t perm, uint32_t *out_faultva)
{
	uint32_t pn, pe, cpe, va;
	vxmem_chunk *chunk = (vxmem_chunk*)mem;
	
	if (addr + len < addr)
		return 0;
	if (len == 0)
		return 1;
	pn = addr / VXPAGESIZE;
	pe = VXPAGEROUND(addr + len-1) / VXPAGESIZE;
	cpe = VXPAGEROUND(chunk->size) / VXPAGESIZE;
	for (; pn < pe; pn++) {
		if (pn >= cpe) {
			if (out_faultva)
				*out_faultva = chunk->size;
			return 0;
		}
		if ((chunk->perm[pn] & perm) != perm) {
			if (out_faultva) {
				va = pn * VXPAGESIZE;
				if (va < addr)
					va = addr;
				*out_faultva = va;
			}
			return 0;
		}
	}
	return 1;
}

static int mmperm(int perm)
{
	int m;
	
	if (perm == 0)
		m = PROT_NONE;
	else {
		m = 0;
		if (perm & VXPERM_READ)
			m |= PROT_READ;
		if (perm & VXPERM_WRITE)
			m |= PROT_WRITE;
		if (perm & VXPERM_EXEC)
			m |= PROT_EXEC;
	}
	return m;
}

static int chunk_setperm(vxmem *mem, uint32_t addr, uint32_t len, uint32_t perm)
{
	vxmem_chunk *chunk = (vxmem_chunk*)mem;
	uint32_t a;
	
	if (addr + len < addr || addr + len > chunk->size) {
		errno = EINVAL;
		return -1;
	}

	if (mem->mapped) {
		if (mprotect(mem->mapped->base + addr, len, mmperm(perm)) < 0)
			return -1;
	}

	for (a = addr; a < addr + len; a += VXPAGESIZE)
		chunk->perm[a/VXPAGESIZE] = perm;
	return 0;
}

static vxmmap *chunk_map(vxmem *mem, uint32_t flags)
{
	vxmmap *mm;
	void *v;
	vxmem_chunk *chunk = (vxmem_chunk*)mem;
	
	if (mem->mapped) {
		mem->mapped->ref++;	// XXX get rid of ref?
		return mem->mapped;
	}

	mm = malloc(sizeof *mm);
	if (mm == NULL)
		return NULL;
	if ((v = mmap(0, chunk->size, PROT_NONE, MAP_32BIT | (vx_elfbigmem ? MAP_PRIVATE : MAP_SHARED), chunk->fd, 0)) == (void*)-1) {
		free(mm);
		return NULL;
	}
//	vxprint("chunk_map %p size %08x\n", v, chunk->size);

	// Now set the permissions on all pages that should be accessible.
	unsigned npages = chunk->size / VXPAGESIZE;
	for (unsigned i = 0; i < npages; ) {
		uint8_t pageperm = chunk->perm[i];

		// For efficiency, merge mprotect calls on page ranges.
		unsigned j;
		for (j = i+1; j < npages; j++)
			if (chunk->perm[j] != pageperm)
				break;

		if (pageperm == 0) {
			i = j;		// nothing to do for this range
			continue;
		}

		// Calculate the effective permission to map with in the host.
		int prot = mmperm(pageperm);
		assert(prot >= 0);

		// Set the permissions on this range.
		if (mprotect((char*)v + i*VXPAGESIZE, (j-i)*VXPAGESIZE, prot) < 0) {
			munmap(v, chunk->size);
			free(mm);
			return NULL;
		}

		i = j;
	}

	mm->base = v;
	mm->size = chunk->size;
	mm->ref = 2;	// XXX get rid of ref?
	mem->mapped = mm;
	return mm;
}

static void chunk_unmap(vxmem *mem, vxmmap *mm)
{
	vxmem_chunk *chunk = (vxmem_chunk*)mem;
	
	if(mm == NULL)
		return;
//	if(--mm->ref > 0)	// XXX get rid of ref?
//		return;
	if(mm == mem->mapped)
		mem->mapped = NULL;
//	vxprint("chunk_unmap %p size %08x\n", mm->base, mm->size);
	munmap(mm->base, mm->size);
	free(mm);
}

static vxmem chunk_proto = 
{
	chunk_read,
	chunk_write,
	chunk_map,
	chunk_unmap,
	chunk_checkperm,
	chunk_setperm,
	chunk_resize,
	chunk_free,
};

static vxmem *vxmem_chunk_fromfd(int fd, off_t size)
{
	vxmem_chunk *chunk;
	
	chunk = calloc(sizeof *chunk, 1);
	if (chunk == NULL)
		return NULL;
	chunk->mem = chunk_proto;
	chunk->fd = fd;
	if (chunk_resize((vxmem*)chunk, size) < 0) {
		free(chunk);
		return NULL;
	}
	return (vxmem*)chunk;
}

vxmem *vxmem_chunk_new(int size)
{
	int fd;
	char tmpfn[] = "/var/tmp/vxXXXXXX";
	vxmem *mem;
	
	if ((fd = mkstemp(tmpfn)) < 0)
		return NULL;
	unlink(tmpfn);
	if ((mem = vxmem_chunk_fromfd(fd, size)) == NULL) {
		close(fd);
		return NULL;
	}
	return mem;
}

