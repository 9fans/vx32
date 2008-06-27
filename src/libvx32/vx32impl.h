#ifndef VX32_IMPL_H
#define VX32_IMPL_H

#include <setjmp.h>
#include <sys/signal.h>
#include <ucontext.h>

// Parameters tweakable for performance
#define VXPROCSMAX	256		// Max # of vxprocs per host proc
#define VXCODEALIGN	16		// Alignment for translated code
#define VXCODEBUFSIZE	(1024*1024)	// Code buffer size (XX make variabale)
#define VXENTRYHASHLEN	32768		// # entrypoints in entry hash table

// VX memory access permissions have a granularity of 4KB pages.
#define VXPAGESHIFT	12
#define VXPAGESIZE	(1 << VXPAGESHIFT)		// 4KB
#define VXPAGETRUNC(v)	((uint32_t)(v) & ~(VXPAGESIZE-1))
#define VXPAGEROUND(v)	VXPAGETRUNC((uint32_t)(v) + (VXPAGESIZE-1))

// The x86 EFLAGS TF bit
#define EFLAGS_TF	0x100

// VX signal handler return values
#define VXSIG_ERROR	0
#define VXSIG_SINGLESTEP	1
#define VXSIG_TRAP	2
#define VXSIG_SAVE_EAX 0x04
#define VXSIG_SAVE_ECX 0x08
#define VXSIG_SAVE_EDX 0x10
#define VXSIG_SAVE_EBX 0x20
#define VXSIG_SAVE_ESP 0x40
#define VXSIG_SAVE_EBP 0x80
#define VXSIG_SAVE_ESI 0x100
#define VXSIG_SAVE_EDI 0x200
#define VXSIG_SAVE_EFLAGS 0x400
#define VXSIG_SAVE_ALL 0x7FC
#define VXSIG_ADD_COUNT_TO_ESP 0x800
#define VXSIG_SAVE_EBX_AS_EIP 0x1000
#define VXSIG_INC_ECX 0x2000

#define VXSIG_COUNT_SHIFT 16

// This is an mmap() flag that we need on 64-bit hosts
// to map our translation state in the low 4GB of address space.
// On 32-bit hosts of course it doesn't exist and isn't needed.
#ifndef MAP_32BIT
#define MAP_32BIT	0
#endif
#define VX_MMAP_FLAGS	MAP_32BIT

// XX for FreeBSD
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

// LDT selectors that we set up on x86-64 hosts
// to allow us to run 32-bit code in compatibility mode.
#define FLATCODE	(0+4+3)		// 4 = LDT, 3 = RPL
#define FLATDATA	(8+4+3)

typedef struct vxinsn vxinsn;

// Translated instruction
struct vxinsn
{
	uint8_t		itype;
	uint8_t		srcofs;		// Offset in original vx32 code
	uint8_t		dstofs;		// Offset in translated x86 code
	uint8_t		dstlen;		// Dest x86 code size for this insn
};

// Instruction types for vxinsn.itype
#define VXI_NOTRANS	0x00		// Simple, no translation required
#define VXI_JUMP	0x02		// Direct jump/branch
#define VXI_CALL	0x03		// Direct call
#define VXI_JUMP8	0x04		// 8-bit jump/branch
#define VXI_JUMPIND	0x05		// Indirect jump
#define VXI_CALLIND	0x06		// Indirect call
#define VXI_RETURN	0x07		// Return
#define VXI_RETURN_IMM	0x08	// Return with immediate pop count
#define VXI_TRAP	0x09		// Trapping instruction
#define VXI_ENDFRAG	0x0A		// Terminate this fragment
#define VXI_LOOP	0x0B	// Loop
#define VXI_LOOPZ	0x0C	// Loopz/loope
#define VXI_LOOPNZ	0x0D	// Loopnz/loopne

// Limits for vxinsn.srcofs and vxinsn.dstofs
#define VXSRCOFS_MAX	255
#define VXDSTOFS_MAX	255

// Each translated fragment consists of:
//	- A struct vxfrag header
//	- A translation summary table containing ninsn vxinsn structs.
//	- The translated code fragment itself, as laid out in the table.
//	- Any trampoline code required by jumps within the fragment.
typedef struct vxfrag {
	uint32_t	eip;		// Original EIP in emulated vx32 code
	uint8_t		ninsn;		// Number of translated vx32 insns
	struct vxinsn	insn[0];	// Beginning of vxinsn table
} vxfrag;

// Macro to find the beginning of translated code in a fragment
#define FRAGCODE(f)	((uint8_t*)&(f)->insn[(f)->ninsn])

#define VXEMU_MAGIC	0xbaf07d5c

// The vx32-EIP-to-translated-entrypoint hash table
// consists of vxentry structures.
// Each vxentry maps a EIP value in the original vx32 code
// to a corresponding EIP in translated code.
// Empty entries are indicated by *dstip* being 0.
// (Zero is theoretically, though unlikely in practice, a valid vx32 EIP.)
typedef struct vxentry {
	uint32_t	srceip;
	uint32_t	dsteip;
} vxentry;

#ifdef __APPLE__
struct i386_thread_state;
int vx32_getcontext(struct i386_thread_state*);
#endif

// Emulation state for vx32-to-x86 translation.
// This is the header for a variable-length structure;
// the variable-length part is the vx32 EIP lookup hash table (etab),
// followed by the instruction translation buffer.
// While running a particular vxproc,
// we always keep the %ds,%es,%ss segment registers loaded with a data segment
// that allows access only to the emulated address space region, and
// we always keep the %fs register loaded with a special data segment
// whose base and limit correspond to the address and size of this state area.
// This way the emulation code can always access the emulation state
// at fixed displacements without having to use any general-purpose registers.
struct vxemu {
	uint32_t	magic;

	// Back pointer to vxproc
	vxproc	*proc;
	vxmmap	*mem;

	// Data segment and vxemu selectors for this vx32 process
	uint32_t	datasel;
	uint32_t	emusel;

	// Cache of what the last modify_ldt set up.
	uintptr_t	ldt_base;
	uint32_t	ldt_size;

	// Pointer to this vxemu struct itself,
	// for use when accessing vxemu struct via %fs segment register.
	uint32_t	emuptr;

	// vx32 virtual CPU state
	vxcpu cpu;
	uint32_t	cpu_trap;	// pending trap
	uint32_t	saved_trap;	// trap to trigger after single-step
	int	nsinglestep;

#if defined(__FreeBSD__)
	mcontext_t		*trapenv;
#elif defined(__APPLE__)
	struct i386_thread_state *trapenv;
#else
	sigjmp_buf	*trapenv;
#endif

	// General emulation state
	uint32_t	emuflags;	// Emulation flags (see below)
	uint32_t	jmpinfo;	// for jump insn backpatching

	// Max # of instructions to execute before returning
	uint32_t	insncount;

	// Instruction scanning/translation state
	struct vxfrag	*txfrag;	// frag currently under construction
	uint8_t		*ininst;	// current instruction being scanned

	// Last guest fragment translated (debugging)
	uint8_t		*guestfrag;
	uint8_t		*guestfragend;

	// Save area for host segment registers while running VX code
	uint16_t	host_ss;	// host ss
	uint16_t	host_ds;	// host ds
	uint16_t	host_es;	// host es
	uint16_t	host_vs;	// host fs/gs (which is os-dependent)

#ifdef	__i386		// x86-32

	// Save area for the host's esp while running emulated code
	uint32_t	host_esp;

#else			// x86-64

	// Save area for the host's rsp while running emulated code
	uint64_t	host_rsp;

	// Far pointers for switching to and from 32-bit compatibility mode
	struct {
		uint32_t ofs;
		uint16_t sel;
	} runptr, retptr;

#endif			// x86-64

	// Translated code buffer.
	// We write code frags into it from the bottom up, increasing codefree.
	// Each frag consists of a vxfrag, a translation summary, and the code.
	// We also keep a table of offsets of all the vxfrag headers,
	// starting at the top of the codebuf and working down, via codetab.
	// The frag header offsets are thus stored in opposite order
	// as the target code fragments themselves,
	// and the codefree and codefrags pointers work towared each other
	// until they collide, at which point the translation buffer is reset.
	void		*codebuf;
	void		*codefree;
	void		*codetab;
	void		*codetop;

	// Hash table for looking up entrypoints in original code
	uint32_t	etablen;	// Number of entries (power of two)
	uint32_t	etabmask;	// etablen-1
	uint32_t	etabcnt;	// Number of entries currently in use
	struct vxentry	etab[0];	// Must be last!
};

// Hash function for entrypoint hash table.
// Keep it consistent with assembly code in x86asm.S!
#define etabhash(va)	(((((va) >> 10) + (va)) >> 10) - (va))

// Emulation flags (emuflags)
#define EMU_FP		0x01	// Enable SSE2 scalar floating-point
#define EMU_VFP		0x02	// Enable vector floating-point
#define EMU_VINT	0x04	// Enable vector integer arithmetic

int vxemu_init(vxproc*);
void vxemu_free(vxemu*);
int vxemu_map(vxemu*, vxmmap*);

void vxemu_flush(vxemu*);

int	vxrun(vxemu*, uint32_t dsteip);
void	vxrun_nullfrag(void);
void	vxrun_setup(vxemu*);
void	vxrun_cleanup(vxemu*);
void	vxprint(char*, ...);

int	vx32_sighandler(int, siginfo_t*, void*);
int	vxemu_sighandler(vxemu*, uint32_t);

#endif  // VX32_IMPL_H

