//
// VX process control interface client stubs
// XX Should this header perhaps be private to the C library?
//
#ifndef _VXPROC_H_
#define _VXPROC_H_

#include <stdint.h>

#include <vx/ipc.h>
#include <vxipc.h>


// Cause proc to exit with specified status code.
// Usually only called by process proc itself.
static inline void
vxproc_exit(intptr_t proc, int status) {
	intptr_t h = proc | VXPROC_EXIT |
			VXIPC_SEND0 | VXIPC_RECV0 | VXIPC_CALL;
	intptr_t s1 = status, s2 = 0;
	VXCALL(h, s1, s2, 0, 0);
}

// Close the handle at address va in proc.
static inline void
vxproc_drop(intptr_t proc, intptr_t va) {
	intptr_t h = proc | VXPROC_DROP |
			VXIPC_SEND0 | VXIPC_RECV0 | VXIPC_CALL;
	intptr_t s1 = va, s2 = 0;
	VXCALL(h, s1, s2, 0, 0);
}

// Extract handle at 'va' from 'proc',
// and place it at 'localva' in current process.
static inline void
vxproc_get(intptr_t proc, intptr_t va, intptr_t localva) {
	intptr_t h = proc | VXPROC_GET |
			VXIPC_SEND0 | VXIPC_RECV1 | VXIPC_CALL;
	intptr_t s1 = va, s2 = 0;
	VXCALL(h, s1, s2, localva, 0);
}

// Insert handle from 'localva' in current process
// into process 'proc' at address 'va'.
static inline void
vxproc_put(intptr_t proc, intptr_t localva, intptr_t va) {
	intptr_t h = proc | VXPROC_PUT |
			VXIPC_SEND1 | VXIPC_RECV0 | VXIPC_CALL;
	intptr_t s1 = localva, s2 = va;
	VXCALL(h, s1, s2, 0, 0);
}

// Allocate a new chunk of memory and place it at 'va' in 'proc'.
static inline void
vxproc_alloc(intptr_t proc, intptr_t va) {
	intptr_t h = proc | VXPROC_ALLOC |
			VXIPC_SEND0 | VXIPC_RECV0 | VXIPC_CALL;
	intptr_t s1 = va, s2 = 0;
	VXCALL(h, s1, s2, 0, 0);
}

// Create a new CALL handle referring to 'proc'
// and place it in the current process at 'localva'.
// Associate opaque value 'id' with new handle
static inline void
vxproc_mkcall(intptr_t proc, intptr_t id, intptr_t localva) {
	intptr_t h = proc | VXPROC_MKCALL |
			VXIPC_SEND0 | VXIPC_RECV1 | VXIPC_CALL;
	intptr_t s1 = id, s2 = 0;
	VXCALL(h, s1, s2, localva, 0);
}


#endif	// _VXPROC_H_
