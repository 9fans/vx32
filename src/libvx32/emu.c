/*
 * Simple instruction scanning and rewriting
 * for implementing vx32 on x86-32 hosts.
 */

#ifdef __APPLE__
#define __DARWIN_UNIX03 0
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <setjmp.h>
#include <assert.h>
#include <errno.h>
#include <sys/stat.h>		// XX FreeBSD 4.9 header bug?
#include <sys/mman.h>
#include <stdarg.h>
#include <unistd.h>

#include "vx32.h"
#include "vx32impl.h"
#include "os.h"
#include "x86dis.h"

// Special values for unused entries in entrypoint hash table
#define NULLSRCEIP		((uint32_t)-1)
#define NULLDSTEIP		((uint32_t)vxrun_nullfrag);

int vx32_debugxlate = 0;

static uint64_t nflush;

static void disassemble(uint8_t *addr0, uint8_t*, uint8_t*);

// Create the emulation state for a new process
int vxemu_init(struct vxproc *vxp)
{
	// Initial emulation hash table size (must be a power of two)
	int etablen = 4096;

	// Allocate the vxemu state area in 32-bit memory,
	// because it must be accessible to our translated code
	// via the special fs segment register setup.
	vxemu *e = mmap(NULL, VXCODEBUFSIZE,
			PROT_READ | PROT_WRITE | PROT_EXEC,
			MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
	if (e == MAP_FAILED){
		vxprint("vxemu_init: mmap failed\n");
		return -1;
	}

	// Basic initialization
	memset(e, 0, sizeof(vxemu));
	e->magic = VXEMU_MAGIC;
	e->proc = vxp;
	vxp->cpu = &e->cpu;
	e->emuptr = (uint32_t)(intptr_t)e;
	e->etablen = etablen;
	e->etabmask = etablen - 1;

	// Initialize the entrypoint table and translation buffer pointers
	vxemu_flush(e);

	vxp->emu = e;
	return 0;
}

void vxemu_free(vxemu *e)
{
	assert(e->proc->emu == e);
	e->proc->emu = NULL;

	// Free the vxemu state area
	munmap(e, VXCODEBUFSIZE);
}

// Reset a vxproc's translation code buffer and entrypoint table.
void vxemu_flush(vxemu *e)
{
	uint32_t i;

	// Clear the entrypoint table.
	uint32_t etablen = e->etablen;
	for (i = 0; i < etablen; i++) {
		e->etab[i].srceip = NULLSRCEIP;
		e->etab[i].dsteip = NULLDSTEIP;
	}
	e->etabcnt = 0;

	// The translated code buffer immediately follows the etab.
	e->codebuf = &e->etab[etablen];
	e->codefree = &e->etab[etablen];
	e->codetab = (void*)e + VXCODEBUFSIZE;
	e->codetop = (void*)e + VXCODEBUFSIZE;

	nflush++;
}

void vxemu_growetab(struct vxemu *e)
{
	// Increase the size of the entrypoint table,
	// which effectively just reserves more memory
	// from the code translation buffer.
	e->etablen *= 2;
	e->etabmask = e->etablen - 1;

	// Re-initialize the entrypoint table and translation buffer.
	vxemu_flush(e);
}

// Each translated frag starts with a one-instruction prolog...
#define PROLOG_LEN		7	// Length of 'mov VSEG:VXEMU_EBX,%ebx'


// Translate a block of code starting at the current vx32 EIP.
// The basic procedure works in four stages.
//
// 1: We first scan the instruction stream to build up a
// tentative vxinsn table for the instructions we plan to translate,
// with output code offsets computed for worst-case instruction lengths.
// This pass handles checking execute permissions on instruction pages,
// and decides exactly how many instructions we'll translate in this block.
// The final instruction in a fragment is always either
// an unconditional flow control instruction (JMP, CALL, RET, INT, etc.),
// or the special "pseudo-instruction" VXI_ENDFRAG,
// which ends the fragment with a jump to the appropriate subsequent EIP.
//
// 2: Next we do a reverse scan through the vxinsn table
// to identify instructions we can simplify:
// particularly instructions with condition code fixups
// whose condition codes are not actually used before they are killed.
// We also identify branches that can be rewritten with 8-bit displacements.
// In the process we adjust the target instruction length (dstlen) fields
// for all simplified instructions accordingly.
//
// 3: We now perform a forward scan through the vxinsn table
// to compute the final offsets for all target instructions in the block.
//
// 4: Finally, we scan the instruction stream again
// and emit the target instructions for the block.
//

// Macros to extract fields in a Mod-Reg-R/M byte
#define EA_MOD(b)	((uint8_t)(b) >> 6)
#define EA_REG(b)	(((uint8_t)(b) >> 3) & 7)
#define EA_RM(b)	((uint8_t)(b) & 7)

// Scan a Mod-Reg-R/M byte and the rest of the effective address
uint8_t *xscan_rm(uint8_t *inp)
{
	uint8_t ea = *inp++;
	switch (EA_MOD(ea)) {
	case 0:
		switch (EA_RM(ea)) {
		case 4:	; // SIB
			uint8_t sib = *inp;
			if ((sib & 7) == 5)
				return inp+1+4;
			else
				return inp+1;
		case 5:	// disp32
			return inp+4;
		default: // [reg]
			return inp;
		}

	case 1:
		switch (EA_RM(ea)) {
		case 4:	// SIB+disp8
			return inp+1+1;
		default: // [reg]+disp8
			return inp+1;
		}

	case 2:
		switch (EA_RM(ea)) {
		case 4: // SIB+disp32
			return inp+1+4;
		default: // [reg]+disp32
			return inp+4;
		}

	case 3:	// reg
		return inp;

	default:
		assert(0);
		return 0;
	}
}

// Translation pass 1:
// scan instruction stream, build preliminary vxinsn table,
// and decide how many instructions to translate in this fragment.
static int xscan(struct vxproc *p)
{
	uint32_t faultva;
	uint32_t eip;
	uint8_t *instart, *inmax;
	struct vxemu *emu = p->emu;

	// Make sure there's enough space in the translated code buffer;
	// if not, then first clear the code buffer and entrypoint table.
	if (((uint8_t*)emu->codetab - (uint8_t*)emu->codefree) < 1024)
		vxemu_flush(emu);

	// Grow the entrypoint hash table if it gets too crowded.
	// This also in effect flushes the translated code buffer.
	if (emu->etabcnt > emu->etablen/2)
		vxemu_growetab(emu);

	// Find and check permissions on the input instruction stream,
	// and determine how far ahead we can scan (up to one full page)
	// before hitting a non-executable page.
	eip = emu->cpu.eip;
	instart = (uint8_t*)emu->mem->base + eip;
	emu->guestfrag = instart;
	if (!vxmem_checkperm(p->mem, eip, 2*VXPAGESIZE, VXPERM_EXEC, &faultva)) {
		if(faultva == eip) {
		noexec:
			emu->cpu_trap = VXTRAP_PAGEFAULT;
			emu->cpu.traperr = 0x10;
			emu->cpu.trapva = faultva;
			return emu->cpu_trap;
		}
	} else
		faultva = VXPAGETRUNC(eip) + 2*VXPAGESIZE;
	inmax = instart + faultva - eip;

	// Create a new fragment header in the code translation buffer
	struct vxfrag *f = (struct vxfrag*)(((intptr_t)emu->codefree + 3) & ~3);
	emu->txfrag = f;
	f->eip = eip;

	unsigned ino = 0;	// instruction number
	unsigned dstofs = PROLOG_LEN;
	uint8_t *inp = instart;
	emu->ininst = inp;	// save instruction currently being translated
	int fin = 0;
	do {
		uint8_t itype = 0;
		uint8_t dstlen;
		uint8_t ea;
		
		if(*inp == 0xF0)	// LOCK
			inp++;

		// Begin instruction decode.
		// We might take a fault on any of these instruction reads
		// if we run off the end of a mapped code page.
		// In that case our exception handler
		// notices that emu->ininst != NULL and initiates recovery.
		// Or we might _not_ take a fault
		// on a page marked read-only but not executable;
		// that's why we check against inmax after each insn.
		switch (*inp++) {

		// OP Eb,Gb; OP Ev,Gv; OP Gb,Eb; OP Gv,Ev
		case 0x00: case 0x01: case 0x02: case 0x03:	// ADD
		case 0x08: case 0x09: case 0x0a: case 0x0b:	// OR
		case 0x10: case 0x11: case 0x12: case 0x13:	// ADC
		case 0x18: case 0x19: case 0x1a: case 0x1b:	// SBB
		case 0x20: case 0x21: case 0x22: case 0x23:	// AND
		case 0x28: case 0x29: case 0x2a: case 0x2b:	// SUB
		case 0x30: case 0x31: case 0x32: case 0x33:	// XOR
		case 0x38: case 0x39: case 0x3a: case 0x3b:	// CMP
		case 0x84: case 0x85:				// TEST
		case 0x86: case 0x87:				// XCHG
		case 0x88: case 0x89: case 0x8a: case 0x8b:	// MOV
			inp = xscan_rm(inp);
			goto notrans;

		// OP AL,Ib; PUSH Ib
		case 0x04: case 0x0c: case 0x14: case 0x1c:	// ADD etc.
		case 0x24: case 0x2c: case 0x34: case 0x3c:	// AND etc.
		case 0x6a:					// PUSH Ib
		case 0xa8:					// TEST AL,Ib
		case 0xb0: case 0xb1: case 0xb2: case 0xb3:	// MOV Gb,Ib
		case 0xb4: case 0xb5: case 0xb6: case 0xb7:
			inp += 1;
			goto notrans;

		// OP EAX,Iv; PUSH Iv; MOV moffs
		case 0x05: case 0x0d: case 0x15: case 0x1d:	// OP EAX,Iv
		case 0x25: case 0x2d: case 0x35: case 0x3d:
		case 0x68:					// PUSH Iv
		case 0xa0: case 0xa1: case 0xa2: case 0xa3:	// MOV moffs
		case 0xa9:					// TEST eAX,Iv
		case 0xb8: case 0xb9: case 0xba: case 0xbb:	// MOV Gv,Iv
		case 0xbc: case 0xbd: case 0xbe: case 0xbf:
			inp += 4;
			goto notrans;

		// CS and DS segment overrides, only valid for branch hints
		case 0x2e:	// CS/"not taken"
		case 0x3e:	// DS/"taken"
			switch (*inp++) {

			// Jcc (8-bit displacement)
			case 0x70: case 0x71: case 0x72: case 0x73:
			case 0x74: case 0x75: case 0x76: case 0x77:
			case 0x78: case 0x79: case 0x7a: case 0x7b:
			case 0x7c: case 0x7d: case 0x7e: case 0x7f:
				inp += 1;
				itype = VXI_JUMP;
				dstlen = 7;	// 32-bit branch w/hint
				goto done;

			// Two-byte opcode
			case 0x0f:
				switch (*inp++) {

				// Jcc - conditional branch with disp32
				case 0x80: case 0x81: case 0x82: case 0x83:
				case 0x84: case 0x85: case 0x86: case 0x87:
				case 0x88: case 0x89: case 0x8a: case 0x8b:
				case 0x8c: case 0x8d: case 0x8e: case 0x8f:
					inp += 4;
					itype = VXI_JUMP;
					dstlen = 7;	// 32-bit branch w/hint
					goto done;

				}
				goto invalid;
			}
			goto invalid;

		// INC reg; DEC reg; PUSH reg; POP reg; XCHG eAX,reg
		case 0x40: case 0x41: case 0x42: case 0x43:	// INC
		case 0x44: case 0x45: case 0x46: case 0x47:
		case 0x48: case 0x49: case 0x4a: case 0x4b:	// DEC
		case 0x4c: case 0x4d: case 0x4e: case 0x4f:
		case 0x50: case 0x51: case 0x52: case 0x53:	// PUSH
		case 0x54: case 0x55: case 0x56: case 0x57:
		case 0x58: case 0x59: case 0x5a: case 0x5b:	// POP
		case 0x5c: case 0x5d: case 0x5e: case 0x5f:
		case 0x90: case 0x91: case 0x92: case 0x93:	// XCHG
		case 0x94: case 0x95: case 0x96: case 0x97:
		case 0x98: case 0x99:				// CWDE, CDQ
		case 0xa4: case 0xa5: case 0xa6: case 0xa7:	// MOVS, CMPS
		case 0xaa: case 0xab:				// STOS
		case 0xac: case 0xad: case 0xae: case 0xaf:	// LODS, SCAS
		case 0xc9:					// LEAVE
		case 0xfc: case 0xfd:				// CLD, STD
			goto notrans;

		// OP Eb,Ib; OP Ev,Ib; IMUL Gv,Ev,Ib
		case 0x80:					// OP Eb,Ib
		case 0x83:					// OP Ev,Ib
		case 0x6b:					// IMUL Gv,Ev,Ib
			inp = xscan_rm(inp);
			inp += 1;
			goto notrans;

		// OP Ev,Iv; IMUL Gv,Ev,Iv
		case 0x81:					// OP Ev,Iv
		case 0x69:					// IMUL Gv,Ev,Iv
			inp = xscan_rm(inp);
			inp += 4;
			goto notrans;

		// Jcc (8-bit displacement)
		case 0x70: case 0x71: case 0x72: case 0x73:
		case 0x74: case 0x75: case 0x76: case 0x77:
		case 0x78: case 0x79: case 0x7a: case 0x7b:
		case 0x7c: case 0x7d: case 0x7e: case 0x7f:
			inp += 1;
			itype = VXI_JUMP;
			dstlen = 6;	// Size of worst-case 32-bit branch
			goto done;

		// LEA Gv,M
		case 0x8d:
			if (EA_MOD(*inp) == 3)	// Mem-only
				goto invalid;
			inp = xscan_rm(inp);
			goto notrans;

		// Group 1a - POP Ev
		case 0x8f:
			if (EA_REG(*inp) != 0)
				goto invalid;
			inp = xscan_rm(inp);
			goto notrans;

		// FWAIT
		case 0x9b:
			if (p->allowfp == 0) {
			badfp:
				if (ino > 0)
					goto endfrag;
				emu->cpu_trap = VXTRAP_FPOFF;
				return emu->cpu_trap;
			}
			goto notrans;

		// PUSHF; POPF
		case 0x9c: case 0x9d:
			assert(0);	// XXX

		// SAHF; LAHF
		case 0x9f: case 0x9e:
			goto notrans;

		// Shift Eb,Ib; Shift Ev,Ib
		case 0xc0: case 0xc1:
			inp = xscan_rm(inp);
			inp += 1;
			// XXX fix CCs
			goto notrans;

		// Shift Eb,1; Shift Ev,1
		case 0xd0: case 0xd1:
			inp = xscan_rm(inp);
			// XXX fix CCs
			goto notrans;

		// Shift Eb,CL; Shift Ev,CL
		case 0xd2: case 0xd3:
			inp = xscan_rm(inp);
			// XXX fix CCs
			goto notrans;

		// RET Iw
		case 0xc2:
			inp += 2;
			itype = VXI_RETURN_IMM;
			dstlen = 7+1+6+5;	// movl %ebx,VSEG:VXEMU_EBX
						// popl %ebx
						// addl $Iw,%esp
						// jmp vxrun_lookup_indirect
			fin = 1;
			goto done;

		// RET
		case 0xc3:
			itype = VXI_RETURN;
			dstlen = 7+1+5;		// movl %ebx,VSEG:VXEMU_EBX
						// popl %ebx
						// jmp vxrun_lookup_indirect
			fin = 1;
			goto done;

		// Group 11 - MOV Eb,Ib
		case 0xc6:
			if (EA_REG(*inp) != 0)
				goto invalid;
			inp = xscan_rm(inp);
			inp += 1;
			goto notrans;

		// Group 11 - MOV Ev,Iv
		case 0xc7:
			if (EA_REG(*inp) != 0)
				goto invalid;
			inp = xscan_rm(inp);
			inp += 4;
			goto notrans;

		// ENTER
		case 0xc8:
			inp += 2+1;		// imm16,imm8
			goto notrans;

		case 0xcd:			// INT n (software interrupt)
			inp++;
		case 0xcc:			// INT3 (breakpoint)
			goto gentrap;

		// 387 escapes - modrm with opcode field
		case 0xd8: case 0xd9: case 0xda: case 0xdb:
		case 0xdc: case 0xdd: case 0xde: case 0xdf:
			if (!p->allowfp)
				goto badfp;
			if ((*inp>>6) == 3)
				inp++;
			else
				inp = xscan_rm(inp);
			goto notrans;

		// Loops
		case 0xe0:	// LOOPNZ cb
			inp++;
			itype = VXI_LOOPNZ;
			dstlen = 3+2+2+5;	// leal -1(ecx), ecx
						// jz .+7
						// jecxz .+5
						// jmp cb
			goto done;

		case 0xe1:	// LOOPZ cb
			inp++;
			itype = VXI_LOOPZ;
			dstlen = 3+2+2+5;	// leal -1(ecx), ecx
						// jnz .+7
						// jecxz .+5
						// jmp cb
			goto done;

		case 0xe2:	// LOOP cb
			inp++;
			itype = VXI_LOOP;
			dstlen = 3+2+5;	// leal -1(ecx), ecx
						// jecxz .+5
						// jmp cb
			goto done;

		// CALL
		case 0xe8:				// CALL Jv
			inp += 4;
			itype = VXI_CALL;
			dstlen = 5+5;		// pushl $nexteip
						// jmp trampoline
			fin = 1;
			goto done;

		// JMP
		case 0xe9:				// JMP Jv
			inp += 4;
			itype = VXI_JUMP;
			dstlen = 5;	// Size of worst-case 32-bit JMP
			fin = 1;
			goto done;

		// JMP short
		case 0xeb:				// JMP Jb
			inp += 1;
			itype = VXI_JUMP;
			dstlen = 5;	// Size of worst-case 32-bit JMP
			fin = 1;
			goto done;

		// Group 3 - unary ops
		case 0xf6:
			ea = *inp;
			inp = xscan_rm(inp);
			switch (EA_REG(ea)) {
			case 0: case 1:			// TEST Eb,Ib
				inp += 1;
			default:			// NOT, NEG, ...
				; // XXX MUL/DIV require fixcc!
			}
			goto notrans;

		case 0xf7:
			ea = *inp;
			inp = xscan_rm(inp);
			switch (EA_REG(ea)) {
			case 0: case 1:			// TEST Ev,Iv
				inp += 4;
			default:			// NOT, NEG, ...
				; // XXX MUL/DIV require fixcc!
			}
			goto notrans;

		// Group 4 - INC, DEC
		case 0xfe:
			ea = *inp;
			inp = xscan_rm(inp);
			switch (EA_REG(ea)) {
			case 0: case 1:			// INC Eb, DEC Eb
				goto notrans;
			}
			goto invalid;

		// Group 5 - INC, DEC, CALL, JMP, PUSH
		case 0xff:
			ea = *inp;
			inp = xscan_rm(inp);
			switch (EA_REG(ea)) {
			case 0: case 1:			// INC Ev, DEC Ev
			case 6:				// PUSH Ev
				goto notrans;
			case 2:				// CALL Ev
				itype = VXI_CALLIND;
				dstlen = 7+(inp-emu->ininst)+5+5;
					// movl %ebx,VSEG:VXEMU_EBX
					// movl <indirect_ea>,%ebx
					//	(same length as CALL inst)
					// pushl $<return_eip>
					// jmp vxrun_lookup_indirect
				fin = 1;
				goto done;
			case 4:				// JMP Ev
				itype = VXI_JUMPIND;
				dstlen = 7+(inp-emu->ininst)+5;
					// movl %ebx,VSEG:VXEMU_EBX
					// movl <indirect_ea>,%ebx
					//	(same length as CALL inst)
					// jmp vxrun_lookup_indirect
				fin = 1;
				goto done;
			}
			goto invalid;
		
		// I/O
		case 0xed:
			goto gentrap;

		// Prefixes
		case 0x0f:	// 2-byte opcode escape
			goto twobyte;
		case 0x66:	// Operand size prefix
			goto opsize;
		case 0xf3:	// REP/REPE prefix
			goto rep;
		case 0xf2:	// REPNE prefix
			goto repne;
		}
		goto invalid;

	// Operand size prefix (0x66) seen
	opsize:
		switch (*inp++) {

		// OP Ev,Gv; OP Gv,Ev
		case 0x01: case 0x03:				// ADD
		case 0x09: case 0x0b:				// OR
		case 0x11: case 0x13:				// ADC
		case 0x19: case 0x1b:				// SBB
		case 0x21: case 0x23:				// AND
		case 0x29: case 0x2b:				// SUB
		case 0x31: case 0x33:				// XOR
		case 0x39: case 0x3b:				// CMP
		case 0x85:					// TEST
		case 0x87:					// XCHG
		case 0x89: case 0x8b:				// MOV
			inp = xscan_rm(inp);
			goto notrans;

		// OP EAX,Iv; PUSH Iv
		case 0x05: case 0x0d: case 0x15: case 0x1d:	// OP EAX,Iv
		case 0x25: case 0x2d: case 0x35: case 0x3d:
		case 0x68:					// PUSH Iv
		case 0xa9:					// TEST eAX,Iv
		case 0xb8: case 0xb9: case 0xba: case 0xbb:	// MOV Gv,Iv
		case 0xbc: case 0xbd: case 0xbe: case 0xbf:
			inp += 2;
			goto notrans;

		// INC reg; DEC reg; PUSH reg; POP reg; XCHG eAX,reg
		case 0x40: case 0x41: case 0x42: case 0x43:	// INC
		case 0x44: case 0x45: case 0x46: case 0x47:
		case 0x48: case 0x49: case 0x4a: case 0x4b:	// DEC
		case 0x4c: case 0x4d: case 0x4e: case 0x4f:
		case 0x50: case 0x51: case 0x52: case 0x53:	// PUSH
		case 0x54: case 0x55: case 0x56: case 0x57:
		case 0x58: case 0x59: case 0x5a: case 0x5b:	// POP
		case 0x5c: case 0x5d: case 0x5e: case 0x5f:
		case 0x90: case 0x91: case 0x92: case 0x93:	// XCHG
		case 0x94: case 0x95: case 0x96: case 0x97:
		case 0x98: case 0x99:				// CWDE, CDQ
		case 0xa4: case 0xa5: case 0xa6: case 0xa7:	// MOVS, CMPS
		case 0xaa: case 0xab:				// STOS
		case 0xac: case 0xad: case 0xae: case 0xaf:	// LODS, SCAS
		case 0xc9:					// LEAVE
		case 0xfc: case 0xfd:				// CLD, STD
			goto notrans;

		// OP Ev,Iv; IMUL Gv,Ev,Iv
		case 0x81:					// OP Ev,Iv
		case 0x69:					// IMUL Gv,Ev,Iv
			inp = xscan_rm(inp);
			inp += 2;
			goto notrans;

		// OP Ev,Ib; IMUL Gv,Ev,Ib
		case 0x83:					// OP Ev,Ib
		case 0x6b:					// IMUL Gv,Ev,Ib
			inp = xscan_rm(inp);
			inp += 1;
			goto notrans;

		// MOV moffs
		case 0xa1: case 0xa3:
			inp += 4;	// always 32-bit offset
			goto notrans;

		// Shift Ev,Ib
		case 0xc1:
			inp = xscan_rm(inp);
			inp += 1;
			// XXX fix CCs
			goto notrans;

		// Shift Ev,1
		case 0xd1:
			inp = xscan_rm(inp);
			// XXX fix CCs
			goto notrans;

		// Shift Ev,CL
		case 0xd3:
			inp = xscan_rm(inp);
			// XXX fix CCs
			goto notrans;

		// Group 11 - MOV Ev,Iv
		case 0xc7:
			if (EA_REG(*inp) != 0)
				goto invalid;
			inp = xscan_rm(inp);
			inp += 2;
			goto notrans;
		
		// Group 3 - unary ops
		case 0xf7:
			ea = *inp;
			inp = xscan_rm(inp);
			switch (EA_REG(ea)) {
			case 0: case 1:			// TEST Ev,Iv
				inp += 2;
			default:			// NOT, NEG, ...
				; // XXX MUL/DIV require fixcc!
			}
			goto notrans;

		// Group 5 - INC, DEC, CALL, JMP, PUSH
		case 0xff:
			ea = *inp;
			inp = xscan_rm(inp);
			switch (EA_REG(ea)) {
			case 0: case 1:			// INC Ev, DEC Ev
				goto notrans;
			}
			goto invalid;

		// Prefixes
		case 0x0f:	// 2-byte opcode escape
			goto twobyte_opsize;
		case 0x66:	// Operand size prefix (redundant)
			goto invalid;
		case 0xf3:	// REP/REPE prefix
			goto opsize_rep;
		case 0xf2:	// REPNE prefix
			goto opsize_repne;
		}
		goto invalid;

	// REP/REPE prefix (0xf3) seen
	rep:
		switch (*inp++) {

		// No-operand insns
		case 0xa4: case 0xa5: case 0xa6: case 0xa7:	// MOVS, CMPS
		case 0xaa: case 0xab:				// STOS
		case 0xac: case 0xad: case 0xae: case 0xaf:	// LODS, SCAS
			goto notrans;

		// Prefixes
		case 0x0f:	// 2-byte opcode escape
			goto twobyte_rep;
		case 0x66:	// Operand size prefix
			goto opsize_rep;
		case 0xf3:	// REP/REPE prefix (redundant)
			goto invalid;
		case 0xf2:	// REPNE prefix (conflicting)
			goto invalid;
		}
		goto invalid;

	// REPNE prefix (0xf2) seen
	repne:
		switch (*inp++) {

		// No-operand insns
		case 0xa6: case 0xa7:				// CMPS
		case 0xae: case 0xaf:				// SCAS
			goto notrans;

		// Prefixes
		case 0x0f:	// 2-byte opcode escape
			goto twobyte_repne;
		case 0x66:	// Operand size prefix
			goto opsize_repne;
		case 0xf3:	// REP/REPE prefix (conflicting)
			goto invalid;
		case 0xf2:	// REPNE prefix (redundant)
			goto invalid;
		}
		goto invalid;


	// Operand size prefix (0x66) and REP/REPE prefix (0xf3) seen
	opsize_rep:
		switch (*inp++) {
		case 0xa5: case 0xa7:				// MOVS, CMPS
		case 0xab:					// STOS
		case 0xad: case 0xaf:				// LODS, SCAS
			goto notrans;
		}
		goto invalid;

	// Operand size prefix (0x66) and REPNE prefix (0xf2) seen
	opsize_repne:
		switch (*inp++) {
		case 0xa7:					// CMPS
		case 0xaf:					// SCAS
			goto notrans;
		}
		goto invalid;


	twobyte:
		switch (*inp++) {

		// SYSCALL instruction for fast system calls
		case 0x05:
			goto gentrap;

		// No additional operand
		case 0xc8: case 0xc9: case 0xca: case 0xcb:	// BSWAP
		case 0xcc: case 0xcd: case 0xce: case 0xcf:
			goto notrans;

		// General EA operands
		case 0x10: case 0x11:			// MOVUPS
		case 0x12:				// MOVLPS Vps,Mq/MOVHLPS
		case 0x14: case 0x15:			// UNPCKLPS/UNPCKHPS
		case 0x16:				// MOVHPS Vps,Mq/MOVLHPS
		case 0x28: case 0x29:			// MOVAPS
		case 0x2e: case 0x2f:			// UCOMISS/COMISS
		case 0x40: case 0x41: case 0x42: case 0x43:	// CMOVcc
		case 0x44: case 0x45: case 0x46: case 0x47:
		case 0x48: case 0x49: case 0x4a: case 0x4b:
		case 0x4c: case 0x4d: case 0x4e: case 0x4f:
		case 0x51:					// SQRTPS
		case 0x54: case 0x55: case 0x56: case 0x57:	// ANDPS etc.
		case 0x58: case 0x59: case 0x5a: case 0x5b:	// ADDPS etc.
		case 0x5c: case 0x5d: case 0x5e: case 0x5f:	// SUBPS etc.
		case 0xa3:					// BT Ev,Gv
		case 0xab:					// BTS Ev,Gv
		case 0xb3:					// BTR Ev,Gv
		case 0xbb:					// BTC Ev,Gv
		case 0xbc: case 0xbd:				// BSF, BSR
		case 0xaf:					// IMUL Gv,Ev
		case 0xb6: case 0xb7:				// MOVZX
		case 0xbe: case 0xbf:				// MOVSX
			inp = xscan_rm(inp);
			goto notrans;

		// General EA operands plus immediate byte
		case 0xc2:				// CMPPS Vps,Wps,Ib
		case 0xc6:				// SHUFPS Vps,Wps,Ib
			inp = xscan_rm(inp);
			inp += 1;
			goto notrans;

		// Memory-only EA operand
		case 0x13:				// MOVLPS Mq,Vps
		case 0x17:				// MOVHPS Mq,Vps
		case 0x2b:				// MOVNTPS
		case 0xc3:				// MOVNTI Md,Gd
			if (EA_MOD(*inp) == 3)	// Mem-only
				goto invalid;
			inp = xscan_rm(inp);
			goto notrans;

		// Register-only EA operand
		case 0x50:				// MOVMSKPS
			if (EA_MOD(*inp) != 3)	// Reg-only
				goto invalid;
			inp = xscan_rm(inp);
			goto notrans;

		// Jcc - conditional branch with disp32
		case 0x80: case 0x81: case 0x82: case 0x83:
		case 0x84: case 0x85: case 0x86: case 0x87:
		case 0x88: case 0x89: case 0x8a: case 0x8b:
		case 0x8c: case 0x8d: case 0x8e: case 0x8f:
			inp += 4;
			itype = VXI_JUMP;
			dstlen = 6;	// Size of worst-case 32-bit branch
			goto done;

		// SETcc - set byte based on condition
		case 0x90: case 0x91: case 0x92: case 0x93:
		case 0x94: case 0x95: case 0x96: case 0x97:
		case 0x98: case 0x99: case 0x9a: case 0x9b:
		case 0x9c: case 0x9d: case 0x9e: case 0x9f:
			if (EA_REG(*inp) != 0)
				goto invalid;
			inp = xscan_rm(inp);
			goto notrans;

		// Shift instructions
		case 0xa4:					// SHLD Ev,Gv,Ib
		case 0xac:					// SHRD Ev,Gv,Ib
			inp = xscan_rm(inp);
			inp += 1;
			// XXX fix cc
			goto notrans;
		case 0xa5:					// SHLD Ev,Gv,CL
		case 0xad:					// SHRD Ev,Gv,CL
			inp = xscan_rm(inp);
			// XXX fix cc
			goto notrans;

		// Group 8 - Bit test/modify with immediate
		case 0xba:
			if (!(EA_REG(*inp) & 4))
				goto invalid;
			inp = xscan_rm(inp);
			inp += 1;
			goto invalid;

		// Group 15 - SSE control
		case 0xae:
			ea = *inp;
			inp = xscan_rm(inp);
			switch (EA_REG(ea)) {
			case 2:					// LDMXCSR
			case 3:					// STMXCSR
				if (EA_MOD(ea) == 3)	// Mem-only
					goto invalid;
				goto notrans;
			// XX LFENCE, SFENCE, MFENCE?
			}
			goto invalid;

		// Group 16 - PREFETCH
		case 0x18:
			if (EA_MOD(*inp) == 3)	// Mem-only
				goto invalid;
			// XX Squash to NOP if EA_REG(*inp) > 3?
			inp = xscan_rm(inp);
			goto notrans;

		}
		goto invalid;

	twobyte_opsize:
		switch (*inp++) {

		// General EA operands
		case 0x10: case 0x11:			// MOVUPD
		case 0x14: case 0x15:			// UNPCKLPD/UNPCKHPD
		case 0x28: case 0x29:			// MOVAPD
		case 0x2e: case 0x2f:			// UCOMISD/COMISD
		case 0x40: case 0x41: case 0x42: case 0x43:	// CMOVcc
		case 0x44: case 0x45: case 0x46: case 0x47:
		case 0x48: case 0x49: case 0x4a: case 0x4b:
		case 0x4c: case 0x4d: case 0x4e: case 0x4f:
		case 0x51:					// SQRTPD
		case 0x54: case 0x55: case 0x56: case 0x57:	// ANDPD etc.
		case 0x58: case 0x59: case 0x5a: case 0x5b:	// ADDPD etc.
		case 0x5c: case 0x5d: case 0x5e: case 0x5f:	// SUBPD etc.
		case 0x60: case 0x61: case 0x62: case 0x63:	// PUNPCK...
		case 0x64: case 0x65: case 0x66: case 0x67:	// PCMPGT...
		case 0x68: case 0x69: case 0x6a: case 0x6b:	// PUNPCK...
		case 0x6c: case 0x6d: case 0x6e: case 0x6f:	// PUNPCK...
		case 0x74: case 0x75: case 0x76:		// PCMPEQ...
		case 0x7e: case 0x7f:				// MOVD/MOVDQA
		case 0xa3:					// BT Ev,Gv
		case 0xab:					// BTS Ev,Gv
		case 0xb3:					// BTR Ev,Gv
		case 0xbb:					// BTC Ev,Gv
		case 0xbc: case 0xbd:				// BSF, BSR
		case 0xaf:					// IMUL Gv,Ev
		case 0xb6:					// MOVZX Gv,Eb
		case 0xbe:					// MOVSX Gv,Eb
		case 0xd1: case 0xd2: case 0xd3:		// PSRLx
		case 0xd4: case 0xd5: case 0xd6:		// PADDQ...
		case 0xd8: case 0xd9: case 0xda: case 0xdb:	// PSUBUSB...
		case 0xdc: case 0xdd: case 0xde: case 0xdf:	// PADDUSB...
		case 0xe0: case 0xe1: case 0xe2: case 0xe3:	// PAVGB...
		case 0xe4: case 0xe5: case 0xe6:		// PMULHUW...
		case 0xe8: case 0xe9: case 0xea: case 0xeb:	// PSUBSB...
		case 0xec: case 0xed: case 0xee: case 0xef:	// PADDSB...
		case 0xf1: case 0xf2: case 0xf3:		// PSLLx
		case 0xf4: case 0xf5: case 0xf6:		// PMULUDQ...
		case 0xf8: case 0xf9: case 0xfa: case 0xfb:	// PSUBB...
		case 0xfc: case 0xfd: case 0xfe:		// PADDB...
			inp = xscan_rm(inp);
			goto notrans;

		// General EA operands plus immediate byte
		case 0xc5:				// PEXTRW Gd,VRdq,Ib
			if (EA_MOD(*inp) != 3)
				goto invalid; // Reg-only
		case 0x70:				// PSHUFD Vdq,Wdq,Ib
		case 0xc2:				// CMPPD Vps,Wps,Ib
		case 0xc4:				// PINSRW Vdq,Ew,Ib
		case 0xc6:				// SHUFPD Vps,Wps,Ib
			inp = xscan_rm(inp);
			inp += 1;
			goto notrans;

		// Memory-only EA operand
		case 0x12: case 0x13:			// MOVLPD
		case 0x16: case 0x17:			// MOVHPD
		case 0x2b:				// MOVNTPD
		case 0xe7:				// MOVNTDQ Mdq,Vdq
			if (EA_MOD(*inp) == 3)		// Mem-only
				goto invalid;
			inp = xscan_rm(inp);
			goto notrans;

		// Register-only EA operand
		case 0x50:				// MOVMSKPD
		case 0xd7:				// PMOVMSKB Gd,VRdq
		case 0xf7:				// MASKMOVQ Vdq,Wdq
			if (EA_MOD(*inp) != 3)		// Reg-only
				goto invalid;
			inp = xscan_rm(inp);
			goto notrans;

		// Shift instructions
		case 0xa4:					// SHLD Ev,Gv,Ib
		case 0xac:					// SHRD Ev,Gv,Ib
			inp = xscan_rm(inp);
			inp += 1;
			// XXX fix cc
			goto notrans;
		case 0xa5:					// SHLD Ev,Gv,CL
		case 0xad:					// SHRD Ev,Gv,CL
			inp = xscan_rm(inp);
			// XXX fix cc
			goto notrans;

		// Group 8 - Bit test/modify with immediate
		case 0xba:
			if (!(EA_REG(*inp) & 4))
				goto invalid;
			inp = xscan_rm(inp);
			inp += 1;
			goto invalid;

		// Group 12, 13, 14 - SSE vector shift w/ immediate
		case 0x71: case 0x72: case 0x73:
			ea = *inp;
			inp = xscan_rm(inp);
			switch (EA_REG(ea)) {
			case 2: case 4: case 6:
				inp += 1;
				goto notrans;
			}
			goto invalid;
		}
		goto invalid;

	twobyte_rep:
		switch (*inp++) {

		// General EA operands
		case 0x10: case 0x11:				// MOVSS
		case 0x2a: case 0x2c: case 0x2d:		// CVT...
		case 0x51:					// SQRTSS
		case 0x58: case 0x59: case 0x5a: case 0x5b:	// ADDSS etc.
		case 0x5c: case 0x5d: case 0x5e: case 0x5f:	// SUBSS etc.
		case 0x6f:					// MOVDQU
		case 0x7e: case 0x7f:				// MOVQ/MOVDQU
		case 0xe6:					// CVTDQ2PD
			inp = xscan_rm(inp);
			goto notrans;

		// General EA operands plus immediate byte
		case 0x70:				// PSHUFHW Vq,Wq,Ib
		case 0xc2:				// CMPSS Vss,Wss,Ib
			inp = xscan_rm(inp);
			inp += 1;
			goto notrans;
		}
		goto invalid;

	twobyte_repne:
		switch (*inp++) {

		// General EA operands
		case 0x10: case 0x11:				// MOVSD
		case 0x2a: case 0x2c: case 0x2d:		// CVT...
		case 0x51:					// SQRTSD
		case 0x58: case 0x59: case 0x5a:		// ADDSD etc.
		case 0x5c: case 0x5d: case 0x5e: case 0x5f:	// SUBSD etc.
		case 0xe6:					// CVTPD2DQ
			inp = xscan_rm(inp);
			goto notrans;

		// General EA operands plus immediate byte
		case 0x70:				// PSHUFLW Vq,Wq,Ib
		case 0xc2:				// CMPSD Vss,Wss,Ib
			inp = xscan_rm(inp);
			inp += 1;
			goto notrans;
		}
		goto invalid;


	invalid:
		vxrun_cleanup(emu);
		vxprint("invalid opcode %02x %02x %02x at eip %08x\n",
			emu->ininst[0], emu->ininst[1], emu->ininst[2],
			emu->cpu.eip + (emu->ininst - instart));
		vxrun_setup(emu);
	gentrap:
		fin = 1;
		itype = VXI_TRAP;
		dstlen = 6+5+11+5;	// movl %eax,VSEG:VXEMU_EAX
					// movl $fin,%eax
					// movl $eip,VSEG:VXEMU_EIP
					// jmp vxrun_gentrap
		goto done;


	notrans:
		// No translation of this instruction is required -
		// dstlen is the same as srclen.
		dstlen = inp - emu->ininst;

	done:
		// Make sure this whole instruction was actually executable
		if (inp > inmax) {
			// If the whole first instruction isn't executable,
			// then just generate the trap immediately,
			// since we know it'll be required.
			if (ino == 0)
				goto noexec;

			// Otherwise, just roll back
			// and stop translating before this instruction,
			// and let the exception (if any)
			// happen next time into the translator.
			goto endfrag;
		}

		// Make sure there's actually room for the resulting code
		if (dstofs + dstlen > VXDSTOFS_MAX) {

			// Roll back and end the frag before this instruction
			endfrag:
			fin = 1;
			itype = VXI_ENDFRAG;
			inp = emu->ininst;	// no source consumed
			dstlen = 5;		// jmp to next frag
		}

		// Record the instruction record
		f->insn[ino].itype = itype;
		f->insn[ino].srcofs = emu->ininst - instart;
		f->insn[ino].dstofs = dstofs;
		f->insn[ino].dstlen = dstlen;

		// Move on to next instruction
		ino++;
		emu->ininst = inp;
		dstofs += dstlen;

	} while (!fin);

	// Record the total number of instructions for this frag
	f->ninsn = ino;
	
// vxprint("%d ins - to %x\n", ino, emu->ininst - instart + eip);
	// Clear the special instruction-scanning exception state flag
	emu->guestfragend = emu->ininst;
	emu->ininst = NULL;

	return 0;
}

// Try to optimize jump instructions whose target
// is in the same fragment we're building.
static inline void xsimp_jump(struct vxproc *p, unsigned ino)
{
	struct vxemu *emu = p->emu;
	struct vxfrag *f = emu->txfrag;
	unsigned ninsn = f->ninsn;
	unsigned srcofs = f->insn[ino].srcofs;
	uint8_t *inp = (uint8_t*)emu->mem->base + emu->cpu.eip + srcofs;

	// Skip any branch prediction hint prefix
	uint8_t opcode = *inp++;
	int dstlen = 2;
	uint32_t targofs = srcofs;
	if (opcode == 0x2e || opcode == 0x3e) {
		opcode = *inp++;
		dstlen = 3;
		targofs++;
	}

	// Determine the jump target.
	if (opcode == 0xe9) {
		// 32-bit JMP
		targofs += 5 + *(int32_t*)inp;
	} else if (opcode == 0x0f) {
		// 32-bit Jcc
		targofs += 6 + *(int32_t*)inp;
	} else {
		// 8-bit JMP or Jcc or LOOP
		targofs += 2 + (int32_t)(int8_t)*inp;
	}
	if (targofs > f->insn[ninsn-1].srcofs)
		return;		// Target is not in this fragment

	// Find the target in the insn table
	unsigned lo = 0;
	unsigned hi = ninsn-1;
	while (hi > lo) {
		unsigned mid = (lo + hi + 1) / 2;
		unsigned midofs = f->insn[mid].srcofs;
		if (targofs >= midofs)
			lo = mid;
		else
			hi = mid - 1;
	}
	if (targofs != f->insn[lo].srcofs)
		return;		// Jump target is _between_ instructions!

	// Make sure target is still in range after translation
	if (lo > ino) {
		if ((int)f->insn[lo].dstofs >
				(int)f->insn[ino+1].dstofs+127)
			return;	// too far ahead
	} else {
		if ((int)f->insn[lo].dstofs <
				(int)f->insn[ino].dstofs+3-128)
			return;	// too far behind
	}

	// In range - convert it to an 8-bit jump!
	f->insn[ino].itype = VXI_JUMP8;
	f->insn[ino].dstlen = dstlen;
}

// Translation pass 2:
// Reverse scan through the instruction table trying to simplify instructions.
static void xsimp(struct vxproc *p)
{
	int i;
	struct vxemu *emu = p->emu;
	struct vxfrag *f = emu->txfrag;
	unsigned ninsn = f->ninsn;

	for (i = ninsn-1; i >= 0; i--) {
		unsigned itype = f->insn[i].itype;

		switch (itype) {
		case VXI_LOOP:
		case VXI_LOOPZ:
		case VXI_LOOPNZ:
		case VXI_JUMP:
			xsimp_jump(p, i);
			break;
		default:
			break;	// no simplifications
		}

	}
}

// Translation pass 3:
// Compute final instruction offsets.
static void xplace(struct vxproc *p)
{
	int i;
	struct vxemu *emu = p->emu;
	struct vxfrag *f = emu->txfrag;
	unsigned ninsn = f->ninsn;

	size_t outofs = PROLOG_LEN;
	for (i = 0; i < ninsn; i++) {
		f->insn[i].dstofs = outofs;
		outofs += f->insn[i].dstlen;
	}
}

// Emit a direct 32-bit jump/branch/call/endfrag instruction.
// The original jump might have been either short or long.
// NB. vxemu_sighandler (sig.c) knows that jumps don't trash registers.
// NB. vxemu_sighandler knows that calls push the return address 
// onto the stack as the first instruction, and that the target address
// can be found at offset 26 of the translation.
static inline void xemit_jump(
		struct vxproc *p, uint8_t itype, unsigned ino,
		uint8_t **extrap)
{
	extern void vxrun_lookup_backpatch();

	struct vxemu *emu = p->emu;
	struct vxfrag *f = emu->txfrag;

	// Determine the jump target EIP
	// and emit the appropriate call/jump/branch instruction,
	// with its target pointing to a temporary jump trampoline.
	uint8_t *tramp = *extrap;
	unsigned srcofs = f->insn[ino].srcofs;
	uint8_t *inp = (uint8_t*)emu->mem->base + emu->cpu.eip + srcofs;
	uint8_t *outp = FRAGCODE(f) + f->insn[ino].dstofs;
	uint32_t targeip = emu->cpu.eip + srcofs;
	if (itype == VXI_JUMP) {

		uint8_t opcode = *inp;

		// Copy any branch taken/not taken hint prefix
		if (opcode == 0x2e || opcode == 0x3e) {
			*outp++ = opcode;
			opcode = *++inp;
			targeip++;
		}

		// Emit the branch/jump/call instruction
		switch (opcode) {

		case 0xe9:	// was a 32-bit JMP
			targeip += 5 + *(int32_t*)&inp[1];
			goto emitjmp;

		case 0xeb:	// was an 8-bit JMP
			targeip += 2 + (int32_t)(int8_t)inp[1];
		emitjmp:
			outp[0] = 0xe9;		// always emit 32-bit JMP
			*(int32_t*)&outp[1] = (int32_t)(tramp - (outp+5));
			outp += 5;
			break;

		case 0x0f:	// was a 32-bit Jcc
			opcode = inp[1];
			targeip += 6 + *(int32_t*)&inp[2];
			goto emitjcc;

		default:	// was an 8-bit Jcc
			opcode = inp[0] + 0x10;
			targeip += 2 + (int32_t)(int8_t)inp[1];
		emitjcc:
			outp[0] = 0x0f;		// always emit 32-bit Jcc
			outp[1] = opcode;
			*(int32_t*)&outp[2] = (int32_t)(tramp - (outp+6));
			outp += 6;
			break;
		}
	} else if (itype == VXI_CALL) {
		assert(*inp == 0xe8);	// 32-bit CALL
		
		outp[0] = 0x68;		// pushl $<return_eip>
		*(uint32_t*)&outp[1] = targeip + 5;
		outp += 5;
		targeip += 5 + *(int32_t*)&inp[1];
		goto emitjmp;
	} else if (itype == VXI_LOOP || itype == VXI_LOOPZ || itype == VXI_LOOPNZ) {
		*outp++ = 0x8d;	// leal -1(ecx) -> ecx
		*outp++ = 0x49;
		*outp++ = 0xff;
		if (itype == VXI_LOOPZ) {
			*outp++ = 0x75;	// jnz .+7
			*outp++ = 0x07;
		} else if (itype == VXI_LOOPNZ) {
			*outp++ = 0x74;	// jz .+7
			*outp++ = 0x07;
		}
		*outp++ = 0xe3;	// jecxz .+5
		*outp++ = 0x05;
		targeip += 2 + (int32_t)(int8_t)inp[1];
		goto emitjmp;
	} else {
		// End-of-fragment pseudo-instruction.
		// targeip already points to the eip we wish to "jump" to.
		assert(itype == VXI_ENDFRAG);
		goto emitjmp;
	}

	// Emit the trampoline code
	tramp[0] = VSEGPREFIX;		// movl $patchrec,VSEG:VXEMU_JMPINFO
	tramp[1] = 0xc7;
	tramp[2] = 0x05;
	*(uint32_t*)&tramp[3] = offsetof(vxemu,jmpinfo);
	*(uint32_t*)&tramp[7] = (uint32_t)((intptr_t)tramp+11+5 -
						(intptr_t)emu);

	tramp[11+0] = 0xe9;		// jmp vxrun_lookup_backpatch
	*(uint32_t*)&tramp[11+1] = (uint32_t)((intptr_t)vxrun_lookup_backpatch
					- (intptr_t)&tramp[11+5]);

	*(uint32_t*)&tramp[11+5] = targeip;		// .long targeip
	*(uint32_t*)&tramp[11+5+4] = (uint32_t)(intptr_t)outp; // .long jmpend
	*extrap = &tramp[11+5+4+4];
}

// Emit a short (8-bit) jump/branch instruction.
// The original branch might have been either short or long.
// NB. vxemu_sighandler (sig.c) knows that jump8s don't
// trash registers.
static inline void xemit_jump8(struct vxproc *p, unsigned ino)
{
	struct vxemu *emu = p->emu;
	struct vxfrag *f = emu->txfrag;
	unsigned srcofs = f->insn[ino].srcofs;
	uint8_t *inp = (uint8_t*)emu->mem->base + emu->cpu.eip + srcofs;
	uint8_t *outp = FRAGCODE(f) + f->insn[ino].dstofs;

	// Copy any branch taken/not taken hint prefix
	uint8_t opcode = *inp;
	int outlen = 2;
	uint32_t targofs = srcofs;
	if (opcode == 0x2e || opcode == 0x3e) {
		*outp++ = opcode;
		opcode = *++inp;
		outlen = 3;
		targofs++;
	}

	// Determine the jump target and output opcode.
	switch (opcode) {
	case 0xe9:	// 32-bit JMP
		opcode = 0xeb;
		targofs += 5 + *(int32_t*)&inp[1];
		break;
	case 0x0f:	// 32-bit Jcc
		opcode = inp[1] - 0x10;
		targofs += 6 + *(int32_t*)&inp[2];
		break;
	case 0xeb:	// 8-bit JMP
	case 0xe0:	// 8-bit LOOP
	case 0xe1:
	case 0xe2:
	default:	// 8-bit Jcc
		targofs += 2 + (int32_t)(int8_t)inp[1];
		break;
	}
	assert(targofs <= f->insn[f->ninsn-1].srcofs);

	// Find the target in the insn table
	unsigned lo = 0;
	unsigned hi = f->ninsn-1;
	while (hi > lo) {
		unsigned mid = (lo + hi + 1) / 2;
		unsigned midofs = f->insn[mid].srcofs;
		if (targofs >= midofs)
			lo = mid;
		else
			hi = mid - 1;
	}
	assert(targofs == f->insn[lo].srcofs);

	// Emit the 2-byte jump instruction (3 bytes with prediction hint)
	outp[0] = opcode;
	outp[1] = (int)f->insn[lo].dstofs - ((int)f->insn[ino].dstofs+outlen);
}

// Emit an indirect jump/call/ret instruction.
// NB. vxemu_sighandler (sig.c) knows that ebx is saved as
// the first instruction and then trashed.  
// NB. vxemu_sighandler knows that the immediate count 
// in a return immediate instruction is at offset 10.
// NB. vxemu_sighandler knows that in an indirect call:
//	* the stack is unchanged until offset -5 (from the end)
//	* at offset -5, the return address has been pushed
//	  and the target eip is in ebx.
static inline void xemit_indir(struct vxproc *p, int itype, unsigned ino)
{
	unsigned i;
	extern void vxrun_lookup_indirect();

	struct vxemu *emu = p->emu;
	struct vxfrag *f = emu->txfrag;
	unsigned srcofs = f->insn[ino].srcofs;
	uint8_t *inp = (uint8_t*)emu->mem->base + emu->cpu.eip + srcofs;
	uint8_t *outp = FRAGCODE(f) + f->insn[ino].dstofs;
	uint8_t *outp0 = outp;

	// Common: movl %ebx,VSEG:VXEMU_EBX
	outp[0] = VSEGPREFIX;		// Appropriate segment override
	outp[1] = 0x89;
	outp[2] = 0x1d;
	*(uint32_t*)&outp[3] = offsetof(vxemu, cpu.reg[EBX]);
	outp += 7;

	// Instruction-specific code
	switch (itype) {
	default:
		assert(0);

	case VXI_CALLIND:
		assert(inp[0] == 0xff);
		assert(EA_REG(inp[1]) == 2);
		goto Common;

	case VXI_JUMPIND:
		assert(inp[0] == 0xff);
		assert(EA_REG(inp[1]) == 4);
	Common:;
		unsigned srclen = xscan_rm(inp+1) - inp;
		outp[0] = 0x8b;		// movl <indirect_ea>,%ebx
		outp[1] = (inp[1] & 0xc7) | (EBX << 3);
		for (i = 2; i < srclen; i++)
			outp[i] = inp[i];
		outp += srclen;
		
		if(itype == VXI_CALLIND) {
			outp[0] = 0x68;		// pushl $<return_eip>
			*(uint32_t*)&outp[1] = emu->cpu.eip + srcofs + srclen;
			outp += 5;
		}
		break;

	case VXI_RETURN:
		assert(inp[0] == 0xc3);
		*outp++ = 0x5b;		// popl %ebx
		break;
	
	case VXI_RETURN_IMM:
		assert(inp[0] == 0xc2);
		outp[0] = 0x5b;		// popl %ebx
		outp[1] = 0x81;		// add $<spc>,%esp
		outp[2] = 0xc4;
		*(uint32_t*)&outp[3] = *(uint16_t*)&inp[1];
		outp += 1+6;
		break;
	}

	// Common: jmp vxrun_lookup_indirect
	outp[0] = 0xe9;
	*(uint32_t*)&outp[1] = (uint32_t)(intptr_t)vxrun_lookup_indirect -
				(uint32_t)(intptr_t)&outp[5];
	outp += 5;
	assert(outp - outp0 == f->insn[ino].dstlen);
}

// NB. vxemu_sighandler (sig.c) knows that eax is saved as
// the first instruction and then trashed.
static void xemit_trap(struct vxproc *p, int ino)
{
	extern void vxrun_gentrap();

	struct vxemu *emu = p->emu;
	struct vxfrag *f = emu->txfrag;

	// Trapping instruction.  Determine the trap type.
	uint32_t trapno;
	uint32_t trapeip = emu->cpu.eip + f->insn[ino].srcofs;
	uint8_t *inp = (uint8_t*)emu->mem->base + trapeip;
	switch (inp[0]) {
	case 0xcc:	// Breakpoint
		trapno = VXTRAP_BREAKPOINT;
		trapeip++;	// EIP points after insn
		break;
	case 0xcd:	// INT $n
		trapno = VXTRAP_SOFT + inp[1];
		trapeip += 2;	// EIP points after insn
		break;
	case 0x0f:
		if (inp[1] == 0x05) {	// SYSCALL instruction
			trapno = VXTRAP_SYSCALL;
			trapeip += 2;	// EIP points after insn
			break;
		}
		// fall thru...
	default:	// Invalid instruction
		trapno = VXTRAP_INVALID;
		break;
	}

	// Emit the output code sequence.
	uint8_t *outp = FRAGCODE(f) + f->insn[ino].dstofs;

	// movl %eax,VSEG:VXEMU_EAX
	outp[0] = VSEGPREFIX;
	outp[1] = 0xa3;
	*(uint32_t*)&outp[2] = offsetof(vxemu, cpu.reg[EAX]);

	// movl $trapno,%eax
	outp[6+0] = 0xb8;
	*(uint32_t*)&outp[6+1] = trapno;

	// movl $trapeip,VSEG:VXEMU_EIP
	outp[6+5+0] = VSEGPREFIX;
	outp[6+5+1] = 0xc7;
	outp[6+5+2] = 0x05;
	*(uint32_t*)&outp[6+5+3] = offsetof(vxemu, cpu.eip);
	*(uint32_t*)&outp[6+5+7] = trapeip;

	// jmp vxrun_gentrap
	outp[6+5+11+0] = 0xe9;
	*(uint32_t*)&outp[6+5+11+1] = (uint32_t)(intptr_t)vxrun_gentrap -
					(uint32_t)(intptr_t)&outp[6+5+11+5];

	assert(f->insn[ino].dstlen == 6+5+11+5);
}

// Translation pass 4:
// Emit the translated instruction stream.
static void xemit(struct vxproc *p)
{
	unsigned i, j;
	struct vxemu *emu = p->emu;
	struct vxfrag *f = emu->txfrag;
	unsigned ninsn = f->ninsn;

	// Writing the instruction stream immediately after the insn table.
	uint8_t *outstart = FRAGCODE(f);

	// Write extra trampoline code after the already-arranged code.
	uint8_t *extra = outstart + (unsigned)f->insn[ninsn-1].dstofs
				+ (unsigned)f->insn[ninsn-1].dstlen;

	// First emit the prolog
	outstart[0] = VSEGPREFIX;			// Segment override
	outstart[1] = 0x8b; outstart[2] = 0x1d;		// movl <abs32>,%ebx
	*(uint32_t*)&outstart[3] = offsetof(vxemu, cpu.reg[EBX]);

	// Now emit the instructions
	asm volatile("cld");
	uint8_t *instart = (uint8_t*)emu->mem->base + emu->cpu.eip;
	for (i = 0; i < ninsn; ) {
		unsigned itype = f->insn[i].itype;

		switch (itype) {

		case VXI_NOTRANS:
			// Just copy strings of untranslated instructions.
			for (j = i+1; j < ninsn; j++)
				if (f->insn[j].itype != VXI_NOTRANS)
					break;

			unsigned srcofs = f->insn[i].srcofs;
			unsigned dstofs = f->insn[i].dstofs;
			uint8_t *inp = instart + f->insn[i].srcofs;
			uint8_t *outp = outstart + f->insn[i].dstofs;
			unsigned cnt = f->insn[j].dstofs - dstofs;
			assert(cnt == f->insn[j].srcofs - srcofs);
			asm volatile("rep movsb"
				: : "c" (cnt), "S" (inp), "D" (outp));

			i = j;
			break;

		case VXI_CALL:
		case VXI_JUMP:
		case VXI_ENDFRAG:
		case VXI_LOOP:
		case VXI_LOOPZ:
		case VXI_LOOPNZ:
			xemit_jump(p, itype, i++, &extra);
			break;

		case VXI_JUMP8:
			xemit_jump8(p, i++);
			break;

		case VXI_RETURN:
		case VXI_JUMPIND:
		case VXI_CALLIND:
			xemit_indir(p, itype, i++);
			break;

		case VXI_TRAP:
			xemit_trap(p, i++);
			break;

		default:
			assert(0);
		}
	}

	// Record the final amount of code table space we've consumed.
	emu->codefree = extra;

	// Add an entry to the code pointer table to the new fragment
	uint32_t *codetab = emu->codetab;
	*--codetab = (uint32_t)(intptr_t)f;
	emu->codetab = codetab;

	assert((void*)extra < (void*)codetab);

	// Insert the new entrypoint into the hash table
	uint32_t idx = etabhash(emu->cpu.eip) & emu->etabmask;
	while (emu->etab[idx].srceip != NULLSRCEIP) {
		assert(emu->etab[idx].srceip != emu->cpu.eip);
		idx = (idx+1) & emu->etabmask;
	}
	emu->etab[idx].srceip = emu->cpu.eip;
	emu->etab[idx].dsteip = (uint32_t)(intptr_t)outstart;
	emu->etabcnt++;
	
	if (vx32_debugxlate) {
		vxrun_cleanup(emu);
		vxprint("====== xlate\n");
		vxprint("-- guest\n");
		disassemble(emu->mem->base, emu->guestfrag, emu->guestfragend);
		vxprint("-- translation\n");
		disassemble(NULL, outstart, extra);
		vxprint("======\n");
		vxrun_setup(emu);
	}
}

static int xlate(struct vxproc *vxp)
{
	// Pass 1: scan instruction stream, build preliminary vxinsn table
	int rc = xscan(vxp);
	if (rc != 0)
		return rc;

	// Pass 2: simplify vxinsns wherever possible
	xsimp(vxp);

	// Pass 3: compute final instruction placement and sizes
	xplace(vxp);

	// Pass 4: emit translated instructions
	xemit(vxp);

	return 0;
}

#if 0
#include <asm/prctl.h>
#include <sys/prctl.h>
#endif

void dumpsegs(const char *prefix)
{
	uint16_t ds, es, fs, gs, ss;
	asm(	"movw %%ds,%0; movw %%es,%1; "
		"movw %%fs,%2; movw %%gs,%3; "
		"movw %%ss,%4"
		: "=rm"(ds), "=rm" (es), "=rm" (fs), "=rm" (gs), "=rm" (ss));
	vxprint("%s: ds=%04x es=%04x fs=%04x gs=%04x ss=%04x\n",
		prefix, ds, es, fs, gs, ss);
#if 0
	unsigned long fsofs, gsofs;
	arch_prctl(ARCH_GET_FS, (unsigned long)&fsofs);
	arch_prctl(ARCH_GET_GS, (unsigned long)&gsofs);
	vxprint("fsofs=%016lx gsofs=%016lx\n", fsofs, gsofs);
#endif
}

int vxproc_run(struct vxproc *vxp)
{
	vxemu *emu = vxp->emu;
	vxmmap *mm;

	// Make sure the process is mapped into our host memory
	if ((mm = vxmem_map(vxp->mem, 0)) == NULL)
		return -1;
	if (vxemu_map(emu, mm) < 0) {
		vxmem_unmap(vxp->mem, mm);
		return -1;
	}
	emu->mem = mm;
	
	// Pending trap?
	if(emu->cpu_trap){
		assert(0);	// Can this even happen?
		int trap = emu->cpu_trap;
		emu->cpu_trap = 0;
		return trap;
	}
	
	uint16_t vs;
	// Registers can't be already loaded or we will smash
	// the "host segment registers" part of emu.
	asm("movw %"VSEGSTR",%0"
		: "=r" (vs));

	assert(vs != emu->emusel);

	// Save our stack environment for exception-handling.
	// This only saves the integer registers.  If the signal handler
	// happens in the middle of a translation involving floating-point
	// code, we need to make sure that when we jump back here in the
	// handler, we first restore the floating point registers to
	// the state they were in during the computation.  (Operating
	// systems typically save the FPU state, reset the FPU, and 
	// pass the saved state to the signal handler.)
	// The Linux signal handler does exactly this.
	//
	// On FreeBSD, after hours wasted trying to manually restore the
	// floating point state, I gave up.  Instead, the FreeBSD code
	// saves an mcontext_t here and then overwrites the signal handler's
	// mcontext_t with this one.  Then when it returns from the handler,
	// the OS will restore the floating point state and then the mcontext,
	// jumping back here with exactly the FPU state that we want.
	// Why not do this on Linux?  Because it didn't work when I tried it,
	// and I was not about to track down why.
	//
	// On OS X, there is no getcontext, so you'd think we'd be back to
	// the Linux approach of manual FPU restore + siglongjmp.
	// Unfortunately, OS X can't deal with siglongjmp from alternate
	// signal stacks.  If it invokes a signal handler on an alternate 
	// signal stack and that handler uses siglongjmp to go back to the
	// original stack instead of returning out of the handler, then
	// OS X thinks the code is still running on the alternate stack, 
	// which causes all sorts of problems.  Thus we have to do the
	// getcontext trick.  Besides, it is far easier to write a getcontext
	// routine--we already need to know the layout of mcontext_t to
	// write the signal handler--than to figure out what the FPU state
	// looks like.
	//
	// And you thought this was going to be easy.

#if defined(__FreeBSD__)
	ucontext_t env;
	emu->trapenv = &env.uc_mcontext;
	volatile int n = 0;
	getcontext(&env);
	if(++n > 1){
#elif defined(__APPLE__)
	struct i386_thread_state env;
	emu->trapenv = &env;
	if(vx32_getcontext(&env)){
#else
	mcontext_t env;
	emu->trapenv = &env;
	if(vx32_getcontext(&env)){
#endif
		if(vx32_debugxlate) vxprint("VX trap %x err %x va %08x "
				"veip %08x veflags %08x\n",
				emu->cpu_trap, emu->cpu.traperr, emu->cpu.trapva,
				emu->cpu.eip, emu->cpu.eflags);
		goto trapped;
	}

	// Load our special vxproc segment selector into fs register.
	vxrun_setup(emu);

	while (1) {
		// Look up the translated entrypoint for the current vx32 EIP.
		uint32_t eip = emu->cpu.eip;
		uint32_t idx = etabhash(eip) & emu->etabmask;
		while (emu->etab[idx].srceip != eip) {
			if (emu->etab[idx].srceip == NULLSRCEIP)
				goto notfound;
			idx = (idx+1) & emu->etabmask;
		}

		// Run the translated code fragment.
		// Return if the code terminated with an exception.
		// Otherwise it terminated because of an untranslated EIP,
		// so translate it.
		if(vxrun(emu, emu->etab[idx].dsteip) != 0)
			break;

	notfound:
		// Translate the code fragment the current emu->cpu.eip points to
		if(xlate(vxp) != 0)
			break;
	}

	// Restore the usual flat model data segment registers.
	vxrun_cleanup(emu);
	
trapped:
	// De-register our setjmp environment for trap handling.
	emu->trapenv = NULL;

	emu->mem = NULL;
	int trap = emu->cpu_trap;
	emu->cpu_trap = 0;
	return trap;
}

void vxemu_stats(struct vxproc *p)
{
	unsigned i;
	vxemu *emu = p->emu;

	vxprint("flush count: %llu\n", nflush);

//	vxprint("vxproc size %dKB\n", p->size/1024);

	unsigned coll = 0;
	for (i = 0; i < emu->etablen; i++) {
		vxentry *e = &emu->etab[i];
		if (e->srceip == NULLSRCEIP)
			continue;
		unsigned idx = etabhash(e->srceip) & emu->etabmask;
		if (idx != i) {
		//	vxprint("srcip %08x hash %d actually at %d\n",
		//		e->srceip, idx, i);
			coll++;
		}
	}
	vxprint("entry tab: %d used, %d total, %d collisions\n",
		emu->etabcnt, emu->etablen, coll);
}

static void disassemble(uint8_t *addr0, uint8_t *p, uint8_t *ep)
{
	xdinst i;
	int j;
	uint8_t *q;
	char buf[128];

	for (; p < ep; p = q) {
		if ((q = x86decode(addr0, p, &i)) == NULL)
			break;
		x86print(buf, sizeof buf, &i);
		vxprint("%08x", i.addr);
		for(j=0; j<i.len; j++)
			vxprint(" %02x", p[j]);
		for(; j<10; j++)
			vxprint("   ");
		vxprint(" %s\n", buf);
	}
}

void vxprint(char *fmt, ...)
{
	va_list arg;
	char buf[512];
	
	va_start(arg, fmt);
	vsnprintf(buf, sizeof buf, fmt, arg);
	va_end(arg);
	write(2, buf, strlen(buf));
}

