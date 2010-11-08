#include <stdlib.h>
#include <errno.h>
#include <assert.h>

#include "vx32.h"
#include "vx32impl.h"

static vxproc *procs[VXPROCSMAX];

vxproc *vxproc_alloc(void)
{
	// Find an available process number
	int pno;
	for (pno = 0; ; pno++) {
		if (pno == VXPROCSMAX) {
			errno = EAGAIN;
			return NULL;
		}
		if (procs[pno] == 0)
			break;
	}

	// Allocate the vxproc structure
	vxproc *pr = calloc(1, sizeof(vxproc));
	if (pr == NULL) {
		errno = ENOMEM;
		return NULL;
	}
	pr->vxpno = pno;

	// Create the process's emulation state
	if (vxemu_init(pr) < 0)
		return NULL;

	procs[pno] = pr;
	return pr;
}

void vxproc_free(vxproc *proc)
{
	assert(procs[proc->vxpno] == proc);
	procs[proc->vxpno] = NULL;
	
	// Free the emulation state
	if (proc->emu != NULL)
		vxemu_free(proc->emu);
	
	// Free the process memory.
	if (proc->mem != NULL)
		vxmem_free(proc->mem);

	free(proc);
}

void vxproc_flush(vxproc *proc)
{
	vxemu_flush(proc->emu);
}
