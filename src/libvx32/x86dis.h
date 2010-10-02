// X86 assembly library, adapted for use in VX32

#include <stdint.h>

typedef struct xdarg xdarg;
typedef struct xdinst xdinst;
typedef struct xinst xinst;

struct xinst
{
	int op;
	int flags;
	int arg1;
	int arg2;
	int arg3;
	xinst *sub;
};

// Decoded instruction argument.
struct xdarg
{
	int op;	// Kind of argument (DA_NONE, ...)
	int reg;
	int index;
	int scale;
	int seg;	// Segment number (>=0) or register (<0).
	uint32_t disp;
};

// Decoded instruction.
struct xdinst
{
	uint32_t addr;
	uint32_t len;
	char *name;			// Mnemonic.
	int opsz;                       // Operand size: 8, 16, 32
	int flags;
	xdarg arg[3];		// Instruction arguments.
};

uint8_t*	x86decode(uint8_t *a0, uint8_t *a, xdinst *dec);
int	x86print(char *buf, int nbuf, xdinst *dec);
