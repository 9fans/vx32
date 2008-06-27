#include <stdlib.h>

#include "vx32.h"

int vxmem_resize(vxmem *mem, size_t newsize)
{
	if(mem->resize == 0)
		abort();
	return mem->resize(mem, newsize);
}

int vxmem_read(vxmem *mem, void *data, uint32_t addr, uint32_t len)
{
	if(mem->read == 0)
		abort();
	return mem->read(mem, data, addr, len);
}

int vxmem_write(vxmem *mem, const void *data, uint32_t addr, uint32_t len)
{
	if(mem->write == 0)
		abort();
	return mem->write(mem, data, addr, len);
}

vxmmap *vxmem_map(vxmem *mem, uint32_t flags)
{
	if(mem->map == 0)
		abort();
	return mem->map(mem, flags);
}

void vxmem_unmap(vxmem *mem, vxmmap *m)
{
	if(mem->unmap == 0)
		abort();
	mem->unmap(mem, m);
}

int vxmem_checkperm(vxmem *mem, uint32_t addr, uint32_t len,
	uint32_t perm, uint32_t *out_faultva)
{
	if(mem->checkperm == 0)
		abort();
	return mem->checkperm(mem, addr, len, perm, out_faultva);
}

int vxmem_setperm(vxmem *mem, uint32_t addr, uint32_t len, uint32_t perm)
{
	if(mem->setperm == 0)
		abort();
	return mem->setperm(mem, addr, len, perm);
}

void vxmem_free(vxmem *mem)
{
	if(mem == 0)
		return;
	if(mem->free == 0)
		abort();
	mem->free(mem);
}

