// VX32 Virtual execution environment

#ifndef VX32_H
#define VX32_H

#include <inttypes.h>
#include <sys/types.h>
#include <sys/signal.h>

// VX memory access permission bits
#define VXPERM_READ  0x01
#define VXPERM_WRITE 0x02
#define VXPERM_EXEC  0x04

// VX trap code categories
#define VXTRAP_CPU		0x000
#define VXTRAP_IRQ		0x100
#define VXTRAP_SOFT		0x200
#define VXTRAP_SYSCALL	0x300
#define VXTRAP_SIGNAL	0x400
#define VXTRAP_SINGLESTEP	0x500
#define VXTRAP_CATEGORY	0xF00

#define VXIRQ_TIMER		0

// VX processor traps (same numbering as Intel)
#define VXTRAP_DIVIDE		0x000	// Integer divide by zero
#define VXTRAP_DEBUG		0x001	// Debug exception (single step)
#define VXTRAP_BREAKPOINT	0x003	// INT3 (breakpoint) instruction
#define VXTRAP_INVALID		0x006	// Invalid instruction opcode
#define VXTRAP_FPOFF		0x007	// Floating point unit needed
#define VXTRAP_PAGEFAULT	0x00e	// Reference to inaccessible page
#define VXTRAP_ALIGN		0x011	// Misaligned memory reference
#define VXTRAP_FLOAT		0x013	// SIMD floating-point exception

// Intel 32-bit register numbers, in Intel order
#define EAX	0
#define ECX	1
#define EDX	2
#define EBX	3
#define ESP	4
#define EBP	5
#define ESI	6
#define EDI	7

// EFLAGS bits implemented in vx32
#define EF_CF_BIT	0
#define EF_PF_BIT	2
#define EF_ZF_BIT	6
#define EF_SF_BIT	7
#define EF_DF_BIT	10
#define EF_OF_BIT	11
#define EF_CF		(1 << EF_CF_BIT)
#define EF_PF		(1 << EF_PF_BIT)
#define EF_ZF		(1 << EF_ZF_BIT)
#define EF_SF		(1 << EF_SF_BIT)
#define EF_DF		(1 << EF_DF_BIT)
#define EF_OF		(1 << EF_OF_BIT)

// Signal handlers.
int vx32_siginit(void);
int vx32_sighandler(int, siginfo_t*, void*);

typedef struct vxcpu	vxcpu;
typedef struct vxemu	vxemu;
typedef struct vxmem	vxmem;
typedef struct vxmmap	vxmmap;
typedef struct vxproc	vxproc;

// VX32 CPU state
struct vxcpu {
	uint32_t	reg[8];
	uint32_t	eip;
	uint32_t	eflags;
	
	uint32_t	traperr;
	uint32_t	trapva;  // cr2
};

// Memory
#define VXMEMMAP_GODMODE	0x01	// Disable permission checking

struct vxmem {
	ssize_t	(*read)(vxmem*, void *data, uint32_t addr, uint32_t len);
	ssize_t (*write)(vxmem*, const void *data, uint32_t addr, uint32_t len);
	vxmmap*	(*map)(vxmem*, uint32_t flags);
	void	(*unmap)(vxmem*, vxmmap*);
	int	(*checkperm)(vxmem*, uint32_t addr, uint32_t len, uint32_t perm, uint32_t *out_faultva);
	int	(*setperm)(vxmem*, uint32_t addr, uint32_t len, uint32_t perm);
	int	(*resize)(vxmem*, size_t);
	void	(*free)(vxmem*);
	
	vxmmap	*mapped;
	vxmmap	*mapped_godmode;
};
int	vxmem_read(vxmem*, void *data, uint32_t addr, uint32_t len);
int	vxmem_write(vxmem*, const void *data, uint32_t addr, uint32_t len);
vxmmap *vxmem_map(vxmem*, uint32_t);
void vxmem_unmap(vxmem*, vxmmap*);
int	vxmem_checkperm(vxmem*, uint32_t addr, uint32_t len, uint32_t perm, uint32_t *out_faultva);
int	vxmem_setperm(vxmem*, uint32_t addr, uint32_t len, uint32_t perm);
void	vxmem_free(vxmem*);
int	vxmem_resize(vxmem*, size_t);

vxmem*	vxmem_chunk_new(int);
vxmem*	vxmem_chunk_copy(vxmem*);

// A single memory-mapped address space region.
struct vxmmap {
	int ref;
	void *base;
	uint32_t size;
};


// Process state.
struct vxproc {
	vxemu	*emu;	// Emulation state
	vxcpu	*cpu;	// Register contents (points into emu)
	vxmem	*mem;	// Memory layout
	int	vxpno;
	int	allowfp;
};

vxproc	*vxproc_alloc(void);
void	vxproc_free(vxproc*);
int	vxproc_run(vxproc *proc);
void	vxproc_flush(vxproc *proc);

// ELF loader
int	vxproc_loadelffile(vxproc *p, const char *file,
	const char *const *argv, const char *const *envp);
int	vxproc_loadelfmem(vxproc *p, const void *mem, size_t nmem,
	const char *const *argv, const char *const *envp);

// VX system call numbers
#define VXPC_EXIT 0x1000
#define VXPC_WRITE 0x3100
#define VXPC_READ 0x03
#define VXPC_SBRK 0x04
#define VXPC_SETPERM 0x01

// VX system call errors
#define VXTRAP_INVARG		0x301	// Invalid argument to system call

typedef int vxpcallhandler(vxproc *proc, void *pcalldata);
int	vxproc_run_pcall(vxproc *proc, int(*)(vxproc*, void*), void*);

extern int vx32_debugxlate;	// dump translations to stderr

#endif
