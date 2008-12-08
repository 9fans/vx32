// X86 assembly library, adapted for use in VX32

#include "x86dis.h"
#include <string.h>
#include <stdio.h>

#define offsetof(S, x) (int)(&((S*)0)->x)
#define nelem(x) (sizeof(x)/sizeof((x)[0]))

typedef unsigned int uint;

enum
{
	O_AAA = 1,
	O_AAD,
	O_AAM,
	O_AAS,
	O_ADC,
	O_ADD,
	O_ADDRSIZE,
	O_AND,
	O_ARPL,
	O_BOUND,
	O_BSF,
	O_BSR,
	O_BT,
	O_BTC,
	O_BTR,
	O_BTS,
	O_CBW,
	O_CLC,
	O_CLD,
	O_CLI,
	O_CLTS,
	O_CMC,
	O_CMP,
	O_CMPS,
	O_CMPXCHG,
	O_CPUID,
	O_CWD,
	O_DAA,
	O_DAS,
	O_DATASIZE,
	O_DEC,
	O_DIV,
	O_ENTER,
	O_HALT,
	O_IDIV,
	O_IMUL,
	O_IMUL2,
	O_IMUL3,
	O_IN,
	O_INC,
	O_INS,
	O_INT,
	O_IRET,
	O_JA,
	O_JC,
	O_JCXZ,
	O_JG,
	O_JGE,
	O_JL,
	O_JLE,
	O_JMP,
	O_JMPF,
	O_JNA,
	O_JNC,
	O_JNO,
	O_JNP,
	O_JNS,
	O_JNZ,
	O_JO,
	O_JP,
	O_JS,
	O_JZ,
	O_LAHF,
	O_LAR,
	O_LDS,
	O_LEA,
	O_LEAVE,
	O_LES,
	O_LFS,
	O_LGDT,
	O_LGS,
	O_LIDT,
	O_LLDT,
	O_LMSW,
	O_LOCK,
	O_LODS,
	O_LOOP,
	O_LOOPNZ,
	O_LOOPZ,
	O_LSL,
	O_LSS,
	O_LTR,
	O_MODRM,
	O_MOV,
	O_MOVS,
	O_MOVSXB,
	O_MOVSXW,
	O_MOVZB,
	O_MOVZW,
	O_MUL,
	O_NEG,
	O_NEXTB,
	O_NOP,
	O_NOT,
	O_OR,
	O_OUT,
	O_OUTS,
	O_POP,
	O_POPA,
	O_POPF,
	O_PUSH,
	O_PUSHA,
	O_PUSHF,
	O_RCL,
	O_RCR,
	O_REP,
	O_REPNE,
	O_RET,
	O_RETF,
	O_ROL,
	O_ROR,
	O_SAHF,
	O_SAR,
	O_SBB,
	O_SCAS,
	O_SEGMENT,
	O_SETA,
	O_SETC,
	O_SETG,
	O_SETGE,
	O_SETL,
	O_SETLE,
	O_SETNA,
	O_SETNC,
	O_SETNO,
	O_SETNP,
	O_SETNS,
	O_SETNZ,
	O_SETO,
	O_SETP,
	O_SETS,
	O_SETZ,
	O_SGDT,
	O_SHL,
	O_SHLD,
	O_SHR,
	O_SHRD,
	O_SIDT,
	O_SLDT,
	O_SMSW,
	O_STC,
	O_STD,
	O_STI,
	O_STOS,
	O_STR,
	O_SUB,
	O_TEST,
	O_VERR,
	O_VERW,
	O_WAIT,
	O_XCHG,
	O_XOR,
	O_CALL,
	O_CALLF,
	O_XLAT,
	
	MAXO
};

enum
{
	F_8 = 1,
	F_16,
	F_32,
	F_1632,
	F_0A,
	F_X,
};

enum
{
	A_0 = 5,
	A_1 = 1,
	A_3 = 3,
	A_4 = 4,

	A_AL = 8,
	A_CL,
	A_DL,
	A_BL,
	A_AH,
	A_CH,
	A_DH,
	A_BH,
	
	A_AX,
	A_CX,
	A_DX,
	A_BX,
	A_SP,
	A_BP,
	A_SI,
	A_DI,
	
	A_EAX,
	A_ECX,
	A_EDX,
	A_EBX,
	A_ESP,
	A_EBP,
	A_ESI,
	A_EDI,
	
	A_ES,
	A_CS,
	A_SS,
	A_DS,
	A_FS,
	A_GS,

	A_IMM8,
	A_IMM16,
	A_IMM32,
	
	A_CR,
	A_DR,
	A_TR,
	A_SR,

	A_M,

	A_MOFF8,
	A_MOFF16,
	A_MOFF32,
	
	A_R8,
	A_R16,
	A_R32,
	
	A_RM8,
	A_RM16,
	A_RM32,
	A_RM32R,
	
	A_REL8,
	A_REL16,
	A_REL32,
	A_IMM1616,
	A_IMM1632,
	
	MAXA
};

enum
{
	D_LOCK = 1<<0,
	D_REP = 1<<1,
	D_REPN = 1<<2,
	D_ES = 1<<3,
	D_CS = 1<<4,
	D_SS = 1<<5,
	D_DS = 1<<6,
	D_FS = 1<<7,
	D_GS = 1<<8,
	D_ADDR32 = 1<<9,
	D_DATA8 = 1<<10,
	D_DATA16 = 1<<11,
	D_DATA32 = 1<<12,
	D_STACK32 = 1<<13,
};

enum
{
	DA_NONE,
	DA_REG8,
	DA_REG16,
	DA_REG32,
	DA_IMM,
	DA_MEM,
	DA_SEGMEM,
	DA_IND16,
	DA_IND32,
	DA_SIB,
	DA_SIBX,
	DA_SEG16,
	DA_REL,
};

// String tables

static char *opnames[MAXO][3] = 
{
	{ "xxx" },
[O_AAA]	{ "aaa" },
[O_AAD]	{ "aad" },
[O_AAM]	{ "aam" },
[O_AAS]	{ "aas" },
[O_ADC]	{ "adcb", "adcw", "adcl" },
[O_ADD]	{ "addb", "addw", "addl" },
[O_ADDRSIZE]	{ "addrsize" },
[O_AND]	{ "andb", "andw", "andl" },
[O_ARPL]	{ "arpl" },
[O_BOUND]	{ 0, "boundw", "boundl" },
[O_BSF]	{ 0, "bsfw", "bsfl" },
[O_BSR]	{ 0, "bsrw", "bsrl" },
[O_BT]	{ 0, "btw", "btl" },
[O_BTC]	{ 0, "btcw", "btcl" },
[O_BTR]	{ 0, "btrw", "btrl" },
[O_BTS]	{ 0, "btsw", "btsl" },
[O_CALL]	{ 0, "call", "call" },
[O_CBW]	{ 0, "cbw", "cwde" },
[O_CLC]	{ "clc" },
[O_CLD]	{ "cld" },
[O_CLI]	{ "cli" },
[O_CLTS]	{ "clts" },
[O_CMC]	{ "cmc" },
[O_CMP]	{ "cmpb", "cmpw", "cmpl" },
[O_CMPS]	{ "cmpsb", "cmpsw", "cmpsl" },
[O_CMPXCHG] { "cmpxchgb", "cmpxchgw", "cmpxchgl" },
[O_CPUID] { "cpuid" },
[O_CWD]	{ 0, "cwd", "cdq" },
[O_DAA]	{ "daa" },
[O_DAS]	{ "das" },
[O_DATASIZE]	{ "datasize" },
[O_DEC]	{ "decb", "decw", "decl" },
[O_DIV]	{ "divb", "divw", "divl" },
[O_ENTER]	{ "enter" },
[O_HALT]	{ "halt" },
[O_IDIV]	{ "idivb", "idivw", "idivl" },
[O_IMUL]	{ "imulb", "imulw", "imull" },
[O_IMUL2]	{ "imulb", "imulw", "imull" },
[O_IMUL3]	{ "imulb", "imulw", "imull" },
[O_IN]	{ "inb", "inw", "inl" },
[O_INC]	{ "incb", "incw", "incl" },
[O_INS]	{ "insb", "insw", "insl" },
[O_INT]	{ "int" },
[O_IRET]	{ 0, "iret", "iretd" },
[O_JA]	{ "ja", "ja", "ja" },
[O_JC]	{ "jc", "jc", "jc" },
[O_JCXZ]	{ "jcxz", "jcxz", "jcxz" },
[O_JG]	{ "jg", "jg", "jg" },
[O_JGE]	{ "jge", "jge", "jge" },
[O_JL]	{ "jl", "jl", "jl" },
[O_JLE]	{ "jle", "jle", "jle" },
[O_JMP]	{ "jmp", "jmp", "jmp" },
[O_JMPF]	{ "ljmp", "ljmp", "ljmp" },
[O_JNA]	{ "jna", "jna", "jna" },
[O_JNC]	{ "jnc", "jnc", "jnc" },
[O_JNO]	{ "jno", "jno", "jno" },
[O_JNP]	{ "jnp", "jnp", "jnp" },
[O_JNS]	{ "jns", "jns", "jns" },
[O_JNZ]	{ "jnz", "jnz", "jnz" },
[O_JO]	{ "jo", "jo", "jo" },
[O_JP]	{ "jp", "jp", "jp" },
[O_JS]	{ "js", "js", "js" },
[O_JZ]	{ "jz", "jz", "jz" },
[O_LAHF]	{ "lahf" },
[O_LAR]	{ 0, "larw", "larl" },
[O_LEA]	{ 0, "leaw", "leal" },
[O_LEAVE]	{ 0, "leavew", "leavel" },
[O_LGDT]	{ "lgdt", },
[O_LDS]	{ 0, "ldsw", "ldsl" },
[O_LES]	{ 0, "lesw", "lesl" },
[O_LFS]	{ 0, "lfsw", "lfsl" },
[O_LGS]	{ 0, "lgsw", "lgsl" },
[O_LSS]	{ 0, "lssw", "lssl" },
[O_LIDT]	{ "lidt" },
[O_LLDT]	{ "lldt" },
[O_LMSW]	{ 0, "lmsw" },
[O_LOCK]	{ "*LOCK*" },
[O_LODS]	{ "lodsb", "lodsw", "lodsl" },
[O_LOOP]	{ "loop" },
[O_LOOPNZ]	{ "loopnz" },
[O_LOOPZ]	{ "loopz" },
[O_LSL]	{ 0, "lslw", "lsll" },
[O_LTR]	{ 0, "ltr", 0 },
[O_MODRM]	{ "*MODRM*" },
[O_MOV]	{ "movb", "movw", "movl" },
[O_MOVS]	{ "movsb", "movsw", "movsl" },
[O_MOVSXB]	{ 0, "movsxb", "movsxb" },
[O_MOVSXW]	{ 0, 0, "movsxw" },
[O_MOVZB]	{ 0, "movzb", "movzb" },
[O_MOVZW]	{ 0, 0, "movzw" },
[O_MUL]	{ "mulb", "mulw", "mull" },
[O_NEG]	{ "negb", "negw", "negl" },
[O_NEXTB]	{ "*NEXTB*" },
[O_NOP]	{ "nop" },
[O_NOT]	{ "notb", "notw", "notl" },
[O_OR]	{ "orb", "orw", "orl" },
[O_OUT]	{ "outb", "outw", "outl" },
[O_OUTS]	{ "outsb", "outsw", "outsl" },
[O_POP]	{ 0, "popw", "popl" },
[O_POPA]	{ 0, "popaw", "popal" },
[O_POPF]	{ 0, "popfw", "popfl" },
[O_PUSH]	{ 0, "pushw", "pushl" },
[O_PUSHA]	{ 0, "pushaw", "pushal" },
[O_PUSHF]	{ 0, "pushfw", "pushfl" },
[O_RCL]	{ "rclb", "rclw", "rcll" },
[O_RCR]	{ "rcrb", "rcrw", "rcrl" },
[O_REP]	{ "rep" },
[O_REPNE]	{ "repne" },
[O_RET]	{ "ret", "ret", "ret" },
[O_RETF]	{ "retf", "retf", "retf" },
[O_ROL]	{ "rolb", "rolw", "roll" },
[O_ROR]	{ "rorb", "rorw", "rorl" },
[O_SAHF]	{ "sahf" },
[O_SAR]	{ "sarb", "sarw", "sarl" },
[O_SBB]	{ "sbbb", "sbbw", "sbbl" },
[O_SCAS]	{ "scasb", "scasw", "scasl" },
[O_SEGMENT]	{ "*SEGMENT*" },
[O_SETA]	{ "seta" },
[O_SETC]	{ "setc" },
[O_SETG]	{ "setg" },
[O_SETGE]	{ "setge" },
[O_SETL]	{ "setl" },
[O_SETLE]	{ "setle" },
[O_SETNA]	{ "setna" },
[O_SETNC]	{ "setnc" },
[O_SETNO]	{ "setno" },
[O_SETNP]	{ "setnp" },
[O_SETNS]	{ "setns" },
[O_SETNZ]	{ "setnz" },
[O_SETO]	{ "seto" },
[O_SETP]	{ "setp" },
[O_SETS]	{ "sets" },
[O_SETZ]	{ "setz" },
[O_SGDT]	{ "sgdt" },
[O_SHL]	{ "shlb", "shlw", "shll" },
[O_SHLD]	{ 0, "shldw", "shldl" },
[O_SHR]	{ "shrb", "shrw", "shrl" },
[O_SHRD]	{ 0, "shrdw", "shrdl" },
[O_SIDT]	{ "sidt" },
[O_SLDT]	{ "sldt" },
[O_SMSW]	{ 0, "smsw" },
[O_STC]	{ "stc" },
[O_STD]	{ "std" },
[O_STI]	{ "sti" },
[O_STOS]	{ "stosb", "stosw", "stosl" },
[O_STR]	{ 0, "str" },
[O_SUB]	{ "subb", "subw", "subl" },
[O_TEST]	{ "testb", "testw", "testl" },
[O_VERR]	{ 0, "verr" },
[O_VERW]	{ 0, "verw" },
[O_WAIT]	{ "wait" },
[O_XCHG]	{ "xchgb", "xchgw", "xchgl" },
[O_XOR]	{ "xorb", "xorw", "xorl" },
[O_XLAT] { "xlat" },
};

static struct {
	int f;
	char *s;
} prefixes[] = {
	D_LOCK, "lock",
	D_REP, "rep",
	D_REPN, "repn",
	D_CS, "cs:",
	D_DS, "ds:",
	D_ES, "es:",
	D_FS, "fs:",
	D_GS, "gs:",
	D_SS, "ss:",
};

static char *reg8[] = { "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh" };
static char *reg16[] = { "ax", "cx", "dx", "bx", "sp", "bp", "si", "di" };
static char *reg32[] = { "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi",
                         "eip", "eflags",
                         "cr0", "cr2", "cr3",
                         "dr0", "dr1", "dr2", "dr3", "dr6", "dr7",
                         "tr6", "tr7" };
static char *seg[] = { "cs", "ds", "es", "fs", "gs", "ss" };

// Register Ids

#define EAX 0
#define ECX 4
#define EDX 8
#define EBX 12
#define ESP 16
#define EBP 20
#define ESI 24
#define EDI 28
#define EIP 32
#define EFLAGS 36
#define CR0 40
#define CR2 44
#define CR3 48
#define DR0 52
#define DR1 56
#define DR2 60
#define DR3 64
#define DR6 68
#define DR7 72
#define TR6 76
#define TR7 80

#define CS 100
#define DS 101
#define ES 102
#define FS 103
#define GS 104
#define SS 105
// Instruction decoding tables.

static xinst inst0F00[] =
{
/*/0*/	{ O_SLDT,	F_16,	A_RM16,		},
	{ O_STR,	F_16,	A_RM16,		},
	{ O_LLDT,	F_16,	A_RM16,		},
	{ O_LTR,	F_16,	A_RM16,		},
	{ O_VERR,	F_16,	A_RM16,		},
	{ O_VERW,	F_16,	A_RM16,		},
	{ -1 },
	{ -1 },
};

static xinst inst0F01[] = 
{
	{ O_SGDT,	F_X,	A_M,		},
	{ O_SIDT,	F_X,	A_M,		},
	{ O_LGDT,	F_X,	A_M,		},
	{ O_LIDT,	F_X,	A_M,		},
	{ O_SMSW,	F_16,	A_RM16,		},
	{ -1 },
	{ O_LMSW,	F_16,	A_RM16,		},
	{ -1 },
};

static xinst inst0FBA[] =
{
	{ -1 },
	{ -1 },
	{ -1 },
	{ -1 },
	{ O_BT,		F_1632,	A_RM16,	A_IMM8 },
	{ O_BTS,	F_1632,	A_RM16,	A_IMM8 },
	{ O_BTR,	F_1632,	A_RM16,	A_IMM8 },
	{ O_BTC,	F_1632,	A_RM16,	A_IMM8 },
};

static xinst inst0F[] =
{
/*00*/	{ O_MODRM,	0,	0,	0,	0,	inst0F00 },
	{ O_MODRM,	0,	0,	0,	0,	inst0F01 },
	{ O_LAR,	F_1632,	A_R16,	A_RM16,	},
	{ O_LSL,	F_1632, A_R16,	A_RM16,	},
	{ -1 },
	{ -1 },
	{ O_CLTS,	F_X,	0,	0,	},
	{ -1 },
/*08*/	{ -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 },
/*10*/	{ -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 },
/*18*/	{ -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 },
/*20*/	{ O_MOV,	F_32,	A_RM32R,	A_CR	},
	{ O_MOV,	F_32,	A_RM32R,	A_DR	},
	{ O_MOV,	F_32,	A_CR,	A_RM32R	},
	{ O_MOV,	F_32,	A_DR,	A_RM32R	},
	{ O_MOV,	F_32,	A_RM32R,	A_TR	},
	{ -1 },
	{ O_MOV,	F_32,	A_TR,	A_RM32R	},
	{ -1 },
/*28*/	{ -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 },
/*30*/	{ -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 },
/*38*/	{ -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 },
/*40*/	{ -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 },
/*48*/	{ -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 },
/*50*/	{ -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 },
/*58*/	{ -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 },
/*60*/	{ -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 },
/*68*/	{ -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 },
/*70*/	{ -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 },
/*78*/	{ -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 },
/*80*/	{ O_JO,		F_1632,	A_REL16,	},
	{ O_JNO,	F_1632,	A_REL16,	},
	{ O_JC,		F_1632,	A_REL16,	},
	{ O_JNC,	F_1632,	A_REL16,	},
	{ O_JZ,		F_1632,	A_REL16,	},
	{ O_JNZ,	F_1632,	A_REL16,	},
	{ O_JNA,	F_1632,	A_REL16,	},
	{ O_JA,		F_1632,	A_REL16,	},
/*88*/	{ O_JS,		F_1632,	A_REL16,	},
	{ O_JNS,	F_1632,	A_REL16,	},
	{ O_JP,		F_1632,	A_REL16,	},
	{ O_JNP,	F_1632,	A_REL16,	},
	{ O_JL,		F_1632,	A_REL16,	},
	{ O_JGE,	F_1632,	A_REL16,	},
	{ O_JLE,	F_1632,	A_REL16,	},
	{ O_JG,		F_1632,	A_REL16,	},
/*90*/	{ O_SETO,	F_8,	A_RM8,	},
	{ O_SETNO,	F_8,	A_RM8,	},
	{ O_SETC,	F_8,	A_RM8,	},
	{ O_SETNC,	F_8,	A_RM8,	},
	{ O_SETZ,	F_8,	A_RM8,	},
	{ O_SETNZ,	F_8,	A_RM8,	},
	{ O_SETNA,	F_8,	A_RM8,	},
	{ O_SETA,	F_8,	A_RM8,	},
/*98*/	{ O_SETS,	F_8,	A_RM8,	},
	{ O_SETNS,	F_8,	A_RM8,	},
	{ O_SETP,	F_8,	A_RM8,	},
	{ O_SETNP,	F_8,	A_RM8,	},
	{ O_SETL,	F_8,	A_RM8,	},
	{ O_SETGE,	F_8,	A_RM8,	},
	{ O_SETLE,	F_8,	A_RM8,	},
	{ O_SETG,	F_8,	A_RM8,	},
/*A0*/	{ O_PUSH,	0,	A_FS,	},
	{ O_POP,	0,	A_FS,	},
	{ O_CPUID, F_X, 0, 0 },
	{ O_BT,		F_1632,	A_RM16,	A_R16 },
	{ O_SHLD,	F_1632,	A_RM16,	A_R16,	A_IMM8 },
	{ O_SHLD,	F_1632,	A_RM16,	A_R16,	A_CL },
	{ -1 },
	{ -1 },
/*A8*/	{ O_PUSH,	0,	A_GS,	},
	{ O_POP,	0,	A_GS,	},
	{ -1 },
	{ O_BTS,	F_1632,	A_RM16,	A_R16 },
	{ O_SHRD,	F_1632,	A_RM16,	A_R16,	A_IMM8 },
	{ O_SHRD,	F_1632,	A_RM16,	A_R16,	A_CL },
	{ -1 },
	{ O_IMUL2,	F_1632,	A_R16,	A_RM16 },
/*B0*/	{ O_CMPXCHG, F_8, A_AL, A_RM8, A_R8 },
	{ O_CMPXCHG, F_1632, A_AX, A_RM16, A_R16 },
	{ O_LSS,	F_1632,	A_R16,	A_M },
	{ O_BTR,	F_1632,	A_RM16,	A_R16 },
	{ O_LFS,	F_1632,	A_R16,	A_M },
	{ O_LGS,	F_1632,	A_R16,	A_M },
	{ O_MOVZB,	F_1632,	A_R16,	A_RM8 },
	{ O_MOVZW,	F_32,	A_R32,	A_RM16 },
/*B8*/	{ -1 },
	{ -1 },
	{ O_MODRM,	0,	0,	0,	0,	inst0FBA },
	{ O_BTC,	F_1632,	A_RM16,	A_R16 },
	{ O_BSF,	F_1632,	A_R16,	A_RM16 },
	{ O_BSR,	F_1632,	A_R16,	A_RM16 },
	{ O_MOVSXB,	F_1632,	A_R16,	A_RM8 },
	{ O_MOVSXW,	F_32,	A_R32,	A_RM16 },
/*C0*/	{ -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 },
/*C8*/	{ -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 },
/*D0*/	{ -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 },
/*D8*/	{ -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 },
/*E0*/	{ -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 },
/*E8*/	{ -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 },
/*F0*/	{ -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 },
/*F8*/	{ -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 },
};

static xinst inst80[] =
{
	{ O_ADD,	F_8,	A_RM8,	A_IMM8 },
	{ O_OR,		F_8,	A_RM8,	A_IMM8 },
	{ O_ADC,	F_8,	A_RM8,	A_IMM8 },
	{ O_SBB,	F_8,	A_RM8,	A_IMM8 },
	{ O_AND,	F_8,	A_RM8,	A_IMM8 },
	{ O_SUB,	F_8,	A_RM8,	A_IMM8 },
	{ O_XOR,	F_8,	A_RM8,	A_IMM8 },
	{ O_CMP,	F_8,	A_RM8,	A_IMM8 },
};

static xinst inst81[] =
{
	{ O_ADD,	F_1632,	A_RM16,	A_IMM16 },
	{ O_OR,		F_1632,	A_RM16,	A_IMM16 },
	{ O_ADC,	F_1632,	A_RM16,	A_IMM16 },
	{ O_SBB,	F_1632,	A_RM16,	A_IMM16 },
	{ O_AND,	F_1632,	A_RM16,	A_IMM16 },
	{ O_SUB,	F_1632,	A_RM16,	A_IMM16 },
	{ O_XOR,	F_1632,	A_RM16,	A_IMM16 },
	{ O_CMP,	F_1632,	A_RM16,	A_IMM16 },
};

static xinst inst83[] =
{
	{ O_ADD,	F_1632,	A_RM16,	A_IMM8 },
	{ O_OR,		F_1632,	A_RM16,	A_IMM8 },
	{ O_ADC,	F_1632,	A_RM16,	A_IMM8 },
	{ O_SBB,	F_1632,	A_RM16,	A_IMM8 },
	{ O_AND,	F_1632,	A_RM16,	A_IMM8 },
	{ O_SUB,	F_1632,	A_RM16,	A_IMM8 },
	{ O_XOR,	F_1632,	A_RM16,	A_IMM8 },
	{ O_CMP,	F_1632,	A_RM16,	A_IMM8 },
};

static xinst inst8F[] = 
{
	{ O_POP,	F_1632,	A_M,	0,	},
	{ -1 },
	{ -1 },
	{ -1 },
	{ -1 },
	{ -1 },
	{ -1 },
	{ -1 },
};

static xinst instC0[] =
{
	{ O_ROL,	F_8,	A_RM8,	A_IMM8,	},
	{ O_ROR,	F_8,	A_RM8,	A_IMM8,	},
	{ O_RCL,	F_8,	A_RM8,	A_IMM8,	},
	{ O_RCR,	F_8,	A_RM8,	A_IMM8,	},
	{ O_SHL,	F_8,	A_RM8,	A_IMM8,	},
	{ O_SHR,	F_8,	A_RM8,	A_IMM8,	},
	{ -1, },
	{ O_SAR,	F_8,	A_RM8,	A_IMM8,	},
};

static xinst instC1[] =
{
	{ O_ROL,	F_1632,	A_RM16,	A_IMM8,	},
	{ O_ROR,	F_1632,	A_RM16,	A_IMM8,	},
	{ O_RCL,	F_1632,	A_RM16,	A_IMM8,	},
	{ O_RCR,	F_1632,	A_RM16,	A_IMM8,	},
	{ O_SHL,	F_1632,	A_RM16,	A_IMM8,	},
	{ O_SHR,	F_1632,	A_RM16,	A_IMM8,	},
	{ -1, },
	{ O_SAR,	F_1632,	A_RM16,	A_IMM8,	},
};

static xinst instD0[] =
{
	{ O_ROL,	F_8,	A_RM8,	A_1,	},
	{ O_ROR,	F_8,	A_RM8,	A_1,	},
	{ O_RCL,	F_8,	A_RM8,	A_1,	},
	{ O_RCR,	F_8,	A_RM8,	A_1,	},
	{ O_SHL,	F_8,	A_RM8,	A_1,	},
	{ O_SHR,	F_8,	A_RM8,	A_1,	},
	{ -1, },
	{ O_SAR,	F_8,	A_RM8,	A_1,	},
};

static xinst instD1[] =
{
	{ O_ROL,	F_1632,	A_RM16,	A_1,	},
	{ O_ROR,	F_1632,	A_RM16,	A_1,	},
	{ O_RCL,	F_1632,	A_RM16,	A_1,	},
	{ O_RCR,	F_1632,	A_RM16,	A_1,	},
	{ O_SHL,	F_1632,	A_RM16,	A_1,	},
	{ O_SHR,	F_1632,	A_RM16,	A_1,	},
	{ -1, },
	{ O_SAR,	F_1632,	A_RM16,	A_1,	},
};

static xinst instD2[] =
{
	{ O_ROL,	F_8,	A_RM8,	A_CL,	},
	{ O_ROR,	F_8,	A_RM8,	A_CL,	},
	{ O_RCL,	F_8,	A_RM8,	A_CL,	},
	{ O_RCR,	F_8,	A_RM8,	A_CL,	},
	{ O_SHL,	F_8,	A_RM8,	A_CL,	},
	{ O_SHR,	F_8,	A_RM8,	A_CL,	},
	{ -1, },
	{ O_SAR,	F_8,	A_RM8,	A_CL,	},
};

static xinst instD3[] =
{
	{ O_ROL,	F_1632,	A_RM16,	A_CL,	},
	{ O_ROR,	F_1632,	A_RM16,	A_CL,	},
	{ O_RCL,	F_1632,	A_RM16,	A_CL,	},
	{ O_RCR,	F_1632,	A_RM16,	A_CL,	},
	{ O_SHL,	F_1632,	A_RM16,	A_CL,	},
	{ O_SHR,	F_1632,	A_RM16,	A_CL,	},
	{ -1, },
	{ O_SAR,	F_1632,	A_RM16,	A_CL,	},
};

static xinst instF6[] =
{
	{ O_TEST,	F_8,	A_RM8,	A_IMM8,	},
	{ -1 },
	{ O_NOT,	F_8,	A_RM8,	0,	},
	{ O_NEG,	F_8,	A_RM8,	0,	},
	{ O_MUL,	F_8,	A_AL,	A_RM8,	},
	{ O_IMUL,	F_8,	A_RM8,	},
	{ O_DIV,	F_8,	A_AX,	A_RM8,	},
	{ O_IDIV,	F_8,	A_AX,	A_RM8,	},
};
static xinst instF7[] =
{
	{ O_TEST,	F_1632,	A_RM16,	A_IMM16,	},
	{ -1 },
	{ O_NOT,	F_1632,	A_RM16,	0,	},
	{ O_NEG,	F_1632,	A_RM16,	0,	},
	{ O_MUL,	F_1632,	A_AX,	A_RM16,	},
	{ O_IMUL,	F_1632,	A_RM16,	},
	{ O_DIV,	F_1632,	A_RM16,	},
	{ O_IDIV,	F_1632,	A_RM16,	},
};

static xinst instFE[] =
{
	{ O_INC,	F_8,	A_RM8,	0,	},
	{ O_DEC,	F_8,	A_RM8,	0,	},
	{ -1 },
	{ -1 },
	{ -1 },
	{ -1 },
	{ -1 },
	{ -1 },
	{ -1 },
	{ -1 },
};

static xinst instFF[] =
{
	{ O_INC,	F_1632,	A_RM16,	0,	},
	{ O_DEC,	F_1632,	A_RM16,	0,	},
	{ O_CALL,	F_1632,	A_RM16,	0,	},
	{ O_CALLF,	F_1632,	A_M,	0,	},	// INDIRECT
	{ O_JMP,	F_1632,	A_RM16,	0,	},
	{ O_JMPF,	F_1632,	A_M,	0,	}, // INDIRECT
	{ O_PUSH,	F_1632,	A_M,	0,	},
	{ -1 },
};

xinst inst[] = 
{
/*00*/	{ O_ADD,	F_8,	A_RM8,	A_R8	},
	{ O_ADD,	F_1632,	A_RM16,	A_R16	},
	{ O_ADD,	F_8,	A_R8,	A_RM8	},
	{ O_ADD,	F_1632,	A_R16,	A_RM16	},
	{ O_ADD,	F_8,	A_AL,	A_IMM8	},
	{ O_ADD,	F_1632,	A_AX,	A_IMM16 },
	{ O_PUSH,	0,	A_ES,		},
	{ O_POP,	0,	A_ES,		},
/*08*/	{ O_OR,		F_8,	A_RM8,	A_R8	},
	{ O_OR,		F_1632,	A_RM16,	A_R16	},
	{ O_OR,		F_8,	A_R8,	A_RM8	},
	{ O_OR,		F_1632,	A_R16,	A_RM16	},
	{ O_OR,		F_8,	A_AL,	A_IMM8	},
	{ O_OR,		F_1632,	A_AX,	A_IMM16 },
	{ O_PUSH,	0,	A_CS,		},
	{ O_NEXTB,	0,	0,	0,	0,	inst0F },
/*10*/	{ O_ADC,	F_8,	A_RM8,	A_R8	},
	{ O_ADC,	F_1632,	A_RM16,	A_R16	},
	{ O_ADC,	F_8,	A_R8,	A_RM8	},
	{ O_ADC,	F_1632,	A_R16,	A_RM16	},
	{ O_ADC,	F_8,	A_AL,	A_IMM8	},
	{ O_ADC,	F_1632,	A_AX,	A_IMM16 },
	{ O_PUSH,	0,	A_SS,		},
	{ O_POP,	0,	A_SS,		},
/*18*/	{ O_SBB,	F_8,	A_RM8,	A_R8	},
	{ O_SBB,	F_1632,	A_RM16,	A_R16	},
	{ O_SBB,	F_8,	A_R8,	A_RM8	},
	{ O_SBB,	F_1632,	A_R16,	A_RM16	},
	{ O_SBB,	F_8,	A_AL,	A_IMM8	},
	{ O_SBB,	F_1632,	A_AX,	A_IMM16 },
	{ O_PUSH,	F_1632,	A_DS,	0,	},
	{ O_POP,	F_1632,	A_DS,	0,	},
/*20*/	{ O_AND,	F_8,	A_RM8,	A_R8	},
	{ O_AND,	F_1632,	A_RM16,	A_R16	},
	{ O_AND,	F_8,	A_R8,	A_RM8	},
	{ O_AND,	F_1632,	A_R16,	A_RM16	},
	{ O_AND,	F_8,	A_AL,	A_IMM8	},
	{ O_AND,	F_1632,	A_AX,	A_IMM16 },
	{ O_SEGMENT,	0,	A_ES,	0,	},
	{ O_DAA,	F_X,	0,	0,	},
/*28*/	{ O_SUB,	F_8,	A_RM8,	A_R8	},
	{ O_SUB,	F_1632,	A_RM16,	A_R16	},
	{ O_SUB,	F_8,	A_R8,	A_RM8	},
	{ O_SUB,	F_1632,	A_R16,	A_RM16	},
	{ O_SUB,	F_8,	A_AL,	A_IMM8	},
	{ O_SUB,	F_1632,	A_AX,	A_IMM16 },
	{ O_SEGMENT,	0,	A_CS,	0,	},
	{ O_DAS,	F_X,	0,	0,	},
/*30*/	{ O_XOR,	F_8,	A_RM8,	A_R8	},
	{ O_XOR,	F_1632,	A_RM16,	A_R16	},
	{ O_XOR,	F_8,	A_R8,	A_RM8	},
	{ O_XOR,	F_1632,	A_R16,	A_RM16	},
	{ O_XOR,	F_8,	A_AL,	A_IMM8	},
	{ O_XOR,	F_1632,	A_AX,	A_IMM16 },
	{ O_SEGMENT,	0,	A_SS,	0,	},
	{ O_AAA,	F_X,	0,	0,	},
/*38*/	{ O_CMP,	F_8,	A_RM8,	A_R8	},
	{ O_CMP,	F_1632,	A_RM16,	A_R16	},
	{ O_CMP,	F_8,	A_R8,	A_RM8	},
	{ O_CMP,	F_1632,	A_R16,	A_RM16	},
	{ O_CMP,	F_8,	A_AL,	A_IMM8	},
	{ O_CMP,	F_1632,	A_AX,	A_IMM16 },
	{ O_SEGMENT,	0,	A_DS,	0,	},
	{ O_AAS,	F_X,	0,	0,	},
/*40*/	{ O_INC,	F_1632,	A_AX,	0,	},	// +r
	{ O_INC,	F_1632,	A_CX,	0,	},
	{ O_INC,	F_1632,	A_DX,	0,	},
	{ O_INC,	F_1632,	A_BX,	0,	},
	{ O_INC,	F_1632,	A_SP,	0,	},
	{ O_INC,	F_1632,	A_BP,	0,	},
	{ O_INC,	F_1632,	A_SI,	0,	},
	{ O_INC,	F_1632,	A_DI,	0,	},
/*48*/	{ O_DEC,	F_1632,	A_AX,	0,	},	// +r
	{ O_DEC,	F_1632,	A_CX,	0,	},
	{ O_DEC,	F_1632,	A_DX,	0,	},
	{ O_DEC,	F_1632,	A_BX,	0,	},
	{ O_DEC,	F_1632,	A_SP,	0,	},
	{ O_DEC,	F_1632,	A_BP,	0,	},
	{ O_DEC,	F_1632,	A_SI,	0,	},
	{ O_DEC,	F_1632,	A_DI,	0,	},
/*50*/	{ O_PUSH,	F_1632,	A_AX,	0,	},	// +r
	{ O_PUSH,	F_1632,	A_CX,	0,	},
	{ O_PUSH,	F_1632,	A_DX,	0,	},
	{ O_PUSH,	F_1632,	A_BX,	0,	},
	{ O_PUSH,	F_1632,	A_SP,	0,	},
	{ O_PUSH,	F_1632,	A_BP,	0,	},
	{ O_PUSH,	F_1632,	A_SI,	0,	},
	{ O_PUSH,	F_1632,	A_DI,	0,	},
/*58*/	{ O_POP,	F_1632,	A_AX,	0,	},	// +r
	{ O_POP,	F_1632,	A_CX,	0,	},
	{ O_POP,	F_1632,	A_DX,	0,	},
	{ O_POP,	F_1632,	A_BX,	0,	},
	{ O_POP,	F_1632,	A_SP,	0,	},
	{ O_POP,	F_1632,	A_BP,	0,	},
	{ O_POP,	F_1632,	A_SI,	0,	},
	{ O_POP,	F_1632,	A_DI,	0,	},
/*60*/	{ O_PUSHA,	F_1632,	0,	0,	},
	{ O_POPA,	F_1632,	0,	0,	},
	{ O_BOUND,	F_1632,	A_R16,	A_M,	},
	{ O_ARPL,	F_X,	A_RM16,	A_R16,	},
	{ O_SEGMENT,	0,	A_FS,	0,	},
	{ O_SEGMENT,	0,	A_GS,	0,	},
	{ O_DATASIZE,	0,	0,	0,	},
	{ O_ADDRSIZE,	0,	0,	0,	},
/*68*/	{ O_PUSH,	F_1632,	A_IMM16,	0,	},
	{ O_IMUL3,	F_1632,	A_R16,	A_RM16, A_IMM16	},
	{ O_PUSH,	F_1632,	A_IMM8,	0,	},
	{ O_IMUL3,	F_1632,	A_R16,	A_RM16, A_IMM8	},
	{ O_INS,	F_8,	0,	0	},
	{ O_INS,	F_1632,	0,	0,	},
	{ O_OUTS,	F_8,	0,	0,	},
	{ O_OUTS,	F_1632,	0,	0,	},
/*70*/	{ O_JO,		F_8,	A_REL8,	},
	{ O_JNO,	F_8,	A_REL8,	},
	{ O_JC,		F_8,	A_REL8,	},
	{ O_JNC,	F_8,	A_REL8,	},
	{ O_JZ,		F_8,	A_REL8,	},
	{ O_JNZ,	F_8,	A_REL8,	},
	{ O_JNA,	F_8,	A_REL8,	},
	{ O_JA,		F_8,	A_REL8,	},
/*78*/	{ O_JS,		F_8,	A_REL8,	},
	{ O_JNS,	F_8,	A_REL8,	},
	{ O_JP,		F_8,	A_REL8,	},
	{ O_JNP,	F_8,	A_REL8,	},
	{ O_JL,		F_8,	A_REL8,	},
	{ O_JGE,	F_8,	A_REL8,	},
	{ O_JLE,	F_8,	A_REL8,	},
	{ O_JG,		F_8,	A_REL8,	},
/*80*/	{ O_MODRM,	0,	0,	0,	0,	inst80 },
	{ O_MODRM,	0,	0,	0,	0,	inst81 },
	{ -1 },
	{ O_MODRM,	0,	0,	0,	0,	inst83 },
	{ O_TEST,	F_8,	A_RM8,	A_R8,	},
	{ O_TEST,	F_1632,	A_RM16,	A_R16,	},
	{ O_XCHG,	F_8,	A_R8,	A_RM8,	},
	{ O_XCHG,	F_1632,	A_R16,	A_RM16,	},
/*88*/	{ O_MOV,	F_8,	A_RM8,	A_R8,	},
	{ O_MOV,	F_1632,	A_RM16,	A_R16,	},
	{ O_MOV,	F_8,	A_R8,	A_RM8,	},
	{ O_MOV,	F_1632,	A_R16,	A_RM16,	},
	{ O_MOV,	F_16,	A_RM16,	A_SR	},
	{ O_LEA,	F_1632,	A_R16,	A_M	},
	{ O_MOV,	F_16,	A_SR,	A_RM16	},
	{ O_MODRM,	0,	0,	0,	0,	inst8F },
/*90*/	{ O_NOP,	F_X,	0,	0,	},	// really XCHG +r
	{ O_XCHG,	F_1632,	A_CX,	A_R16	},
	{ O_XCHG,	F_1632,	A_DX,	A_R16	},
	{ O_XCHG,	F_1632,	A_BX,	A_R16	},
	{ O_XCHG,	F_1632,	A_SP,	A_R16	},
	{ O_XCHG,	F_1632,	A_BP,	A_R16	},
	{ O_XCHG,	F_1632,	A_SI,	A_R16	},
	{ O_XCHG,	F_1632,	A_DI,	A_R16	},
/*98*/	{ O_CBW,	F_1632,	0,	0	},	// or CWDE
	{ O_CWD,	F_1632,	0,	0,	},	// or CDQ
	{ O_CALLF,	F_1632,	A_IMM1616,	0,	},
	{ O_WAIT,	F_X,	0,	0,	},
	{ O_PUSHF,	F_1632,	0,	0,	},
	{ O_POPF,	F_1632,	0,	0,	},
	{ O_SAHF,	F_X,	0,	0,	},
	{ O_LAHF,	F_X,	0,	0,	},
/*A0*/	{ O_MOV,	F_8,	A_AL,	A_MOFF8	},
	{ O_MOV,	F_1632,	A_AX,	A_MOFF16},
	{ O_MOV,	F_8,	A_MOFF8,A_AL	},
	{ O_MOV,	F_1632,	A_MOFF16,	A_AX},
	{ O_MOVS,	F_8,	0,	0	},
	{ O_MOVS,	F_1632,	0,	0,	},
	{ O_CMPS,	F_8,	0,	0	},
	{ O_CMPS,	F_1632,	0,	0,	},
/*A8*/	{ O_TEST,	F_8,	A_AL,	A_IMM8	},
	{ O_TEST,	F_1632,	A_AX,	A_IMM16	},
	{ O_STOS,	F_8,	0,	0	},
	{ O_STOS,	F_1632,	0,	0,	},
	{ O_LODS,	F_8,	0,	0	},
	{ O_LODS,	F_1632,	0,	0,	},
	{ O_SCAS,	F_8,	0,	0	},
	{ O_SCAS,	F_1632,	0,	0,	},
/*B0*/	{ O_MOV,	F_8,	A_AL,	A_IMM8,	},	// +r
	{ O_MOV,	F_8,	A_CL,	A_IMM8,	},
	{ O_MOV,	F_8,	A_DL,	A_IMM8,	},
	{ O_MOV,	F_8,	A_BL,	A_IMM8,	},
	{ O_MOV,	F_8,	A_AH,	A_IMM8,	},
	{ O_MOV,	F_8,	A_CH,	A_IMM8,	},
	{ O_MOV,	F_8,	A_DH,	A_IMM8,	},
	{ O_MOV,	F_8,	A_BH,	A_IMM8,	},
/*B8*/	{ O_MOV,	F_1632,	A_AX,	A_IMM16,	},	// +r
	{ O_MOV,	F_1632,	A_CX,	A_IMM16,	},
	{ O_MOV,	F_1632,	A_DX,	A_IMM16,	},
	{ O_MOV,	F_1632,	A_BX,	A_IMM16,	},
	{ O_MOV,	F_1632,	A_SP,	A_IMM16,	},
	{ O_MOV,	F_1632,	A_BP,	A_IMM16,	},
	{ O_MOV,	F_1632,	A_SI,	A_IMM16,	},
	{ O_MOV,	F_1632,	A_DI,	A_IMM16,	},
/*C0*/	{ O_MODRM,	0,	0,	0,	0,	instC0 },
	{ O_MODRM,	0,	0,	0,	0,	instC1 },
	{ O_RET,	0,	A_IMM16,	0,	},
	{ O_RET,	0,	0,	0,	},
	{ O_LES,	F_1632,	A_R16,	A_M },
	{ O_LDS,	F_1632,	A_R16,	A_M },
	{ O_MOV,	F_8,	A_RM8,	A_IMM8	},
	{ O_MOV,	F_1632,	A_RM16,	A_IMM16	},
/*C8*/	{ O_ENTER,	0,	A_IMM16,	A_IMM8 },
	{ O_LEAVE,	F_1632,	0,	0,	},
	{ O_RETF,	0,	A_IMM16,	0,	},
	{ O_RETF,	0,	0,	0,	},
	{ O_INT,	F_X,	A_3,	0,	},
	{ O_INT,	F_X,	A_IMM8,	0,	},
	{ O_INT,	F_X,	A_4,	0,	},
	{ O_IRET,	F_1632,	0,	0,	},
/*D0*/	{ O_MODRM,	0,	0,	0,	0,	instD0 },
	{ O_MODRM,	0,	0,	0,	0,	instD1 },
	{ O_MODRM,	0,	0,	0,	0,	instD2 },
	{ O_MODRM,	0,	0,	0,	0,	instD3 },
	{ O_AAM,	F_0A,	0,	0,	},
	{ O_AAD,	F_0A,	0,	0,	},
	{ -1 },
	{ O_XLAT,       F_8 },
/*D8*/	{ -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 },
/*E0*/	{ O_LOOPNZ,	F_8,	A_REL8,	0,	},
	{ O_LOOPZ,	F_8,	A_REL8,	0,	},
	{ O_LOOP,	F_8,	A_REL8,	0,	},
	{ O_JCXZ,	F_8,	A_REL8,	0,	},
	{ O_IN,		F_8,	A_AL,	A_IMM8,	},
	{ O_IN,		F_1632,	A_AX,	A_IMM8,	},
	{ O_OUT,	F_8,	A_IMM8,	A_AL,	},
	{ O_OUT,	F_1632,	A_IMM8,	A_AX,	},
/*E8*/	{ O_CALL,	F_1632,	A_REL16,	0,	},
	{ O_JMP,	F_1632,	A_REL16,	0,	},
	{ O_JMPF,	F_1632,	A_IMM1616,	0,	},
	{ O_JMP,	F_8,	A_REL8,	0,	},
	{ O_IN,		F_8,	A_AL,	A_DX,	},
	{ O_IN,		F_1632,	A_AX,	A_DX,	},	// DX not part of 16 -> 32
	{ O_OUT,	F_8,	A_DX,	A_AL,	},
	{ O_OUT,	F_1632,	A_DX,	A_AX,	},
/*F0*/	{ O_LOCK,	0,	0,	0,	},
	{ -1 },
	{ O_REPNE,	0,	0,	0,	},
	{ O_REP,	0,	0,	0,	},	// or O_REPE depending on next inst
	{ O_HALT,	F_X,	0,	0,	},
	{ O_CMC,	F_X,	0,	0,	},
	{ O_MODRM,	0,	0,	0,	0,	instF6 },
	{ O_MODRM,	0,	0,	0,	0,	instF7 },
/*F8*/	{ O_CLC,	F_X,	0,	0,	},
	{ O_STC,	F_X,	0,	0,	},
	{ O_CLI,	F_X,	0,	0,	},
	{ O_STI,	F_X,	0,	0,	},
	{ O_CLD,	F_X,	0,	0,	},
	{ O_STD,	F_X,	0,	0,	},
	{ O_MODRM,	0,	0,	0,	0,	instFE },
	{ O_MODRM,	0,	0,	0,	0,	instFF },
};

// Argument decoding tables.

static int needsmodrm[MAXA] =
{
[A_CR]		1,
[A_DR]		1,
[A_TR]		1,
[A_SR]		1,

[A_R8]		1,
[A_R16]		1,
[A_R32]		1,

[A_M]	1,

[A_RM8]		1,
[A_RM16]	1,
[A_RM32]	1,
};

static int cvt32[MAXA] =
{
[A_AX]		A_EAX,
[A_CX]		A_ECX,
[A_DX]		A_EDX,
[A_BX]		A_EBX,
[A_SP]		A_ESP,
[A_BP]		A_EBP,
[A_SI]		A_ESI,
[A_DI]		A_EDI,	
[A_IMM16]	A_IMM32,
[A_R16]		A_R32,	
[A_RM16]	A_RM32,	
[A_REL16]	A_REL32,
[A_IMM1616]	A_IMM1632,
};

static int acvt32[MAXA] =
{
[A_MOFF16]	A_MOFF32,	
};


// Addressing mode tables.

static xdarg daimm = { DA_IMM };
static xdarg darel = { DA_REL };

static xdarg daseg[] = {
	{ DA_SEG16, ES },
	{ DA_SEG16, CS },
	{ DA_SEG16, SS },
	{ DA_SEG16, DS },
	{ DA_SEG16, FS },
	{ DA_SEG16, GS },
};

#define L 0	// offset of al in eax
#define H 1	// offset of ah in eax
#define X 0	// offset of ax in eax

static xdarg dareg[] = {
	{ DA_REG8, EAX+L },
	{ DA_REG8, ECX+L },
	{ DA_REG8, EDX+L },
	{ DA_REG8, EBX+L },
	{ DA_REG8, EAX+H },
	{ DA_REG8, ECX+H },
	{ DA_REG8, EDX+H },
	{ DA_REG8, EBX+H },
	
	{ DA_REG16, EAX+X },
	{ DA_REG16, ECX+X },
	{ DA_REG16, EDX+X },
	{ DA_REG16, EBX+X },
	{ DA_REG16, ESP+X },
	{ DA_REG16, EBP+X },
	{ DA_REG16, ESI+X },
	{ DA_REG16, EDI+X },
	
	{ DA_REG32, EAX },
	{ DA_REG32, ECX },
	{ DA_REG32, EDX },
	{ DA_REG32, EBX },
	{ DA_REG32, ESP },
	{ DA_REG32, EBP },
	{ DA_REG32, ESI },
	{ DA_REG32, EDI },
};

static xdarg dacr[] = {
	{ DA_REG32, CR0 },
	{ DA_NONE },
	{ DA_REG32, CR2 },
	{ DA_REG32, CR3 },
	{ DA_NONE },
	{ DA_NONE },
	{ DA_NONE },
	{ DA_NONE },
};

static xdarg dadr[] = {
	{ DA_REG32, DR0 },
	{ DA_REG32, DR1 },
	{ DA_REG32, DR2 },
	{ DA_REG32, DR3 },
	{ DA_NONE },
	{ DA_NONE },
	{ DA_REG32, DR6 },
	{ DA_REG32, DR7 },
};

static xdarg datr[] = {
	{ DA_NONE },
	{ DA_NONE },
	{ DA_NONE },
	{ DA_NONE },
	{ DA_NONE },
	{ DA_NONE },
	{ DA_REG32, TR6 },
	{ DA_REG32, TR7 },
};

static xdarg daind16[] = {
	{ DA_IND16, EBX+X, ESI+X, 1, -DS },
	{ DA_IND16, EBX+X, EDI+X, 1, -DS },
	{ DA_IND16, EBP+X, ESI+X, 1, -SS },
	{ DA_IND16, EBP+X, EDI+X, 1, -SS },
	{ DA_IND16, ESI+X, 0, 0, -DS },
	{ DA_IND16, EDI+X, 0, 0, -DS },
	{ DA_IND16, EBP+X, 0, 0, -SS },
	{ DA_IND16, EBX+X, 0, 0, -DS },

	{ DA_IND16, EBX+X, ESI+X, 1, -DS },
	{ DA_IND16, EBX+X, EDI+X, 1, -DS },
	{ DA_IND16, EBP+X, ESI+X, 1, -SS },
	{ DA_IND16, EBP+X, EDI+X, 1, -DS },
	{ DA_IND16, ESI+X, 0, 0, -DS },
	{ DA_IND16, EDI+X, 0, 0, -DS },
	{ DA_IND16, EBP+X, 0, 0, -SS },
	{ DA_IND16, EBX+X, 0, 0, -DS },

	{ DA_IND16, EBX+X, ESI+X, 1, -DS },
	{ DA_IND16, EBX+X, EDI+X, 1, -DS },
	{ DA_IND16, EBP+X, ESI+X, 1, -SS },
	{ DA_IND16, EBP+X, EDI+X, 1, -DS },
	{ DA_IND16, ESI+X, 0, 0, -DS },
	{ DA_IND16, EDI+X, 0, 0, -DS },
	{ DA_IND16, EBP+X, 0, 0, -SS },
	{ DA_IND16, EBX+X, 0, 0, -DS },
};

static xdarg daind32[] = {
	{ DA_IND32, EAX, 0, 0, -DS },
	{ DA_IND32, ECX, 0, 0, -DS },
	{ DA_IND32, EDX, 0, 0, -DS },
	{ DA_IND32, EBX, 0, 0, -DS },
	{ DA_NONE },
	{ DA_IND32, EBP, 0, 0, -SS },
	{ DA_IND32, ESI, 0, 0, -DS },
	{ DA_IND32, EDI, 0, 0, -DS },

	{ DA_IND32, EAX, 0, 0, -DS },
	{ DA_IND32, ECX, 0, 0, -DS },
	{ DA_IND32, EDX, 0, 0, -DS },
	{ DA_IND32, EBX, 0, 0, -DS },
	{ DA_NONE },
	{ DA_IND32, EBP, 0, 0, -SS },
	{ DA_IND32, ESI, 0, 0, -DS },
	{ DA_IND32, EDI, 0, 0, -DS },

	{ DA_IND32, EAX, 0, 0, -DS },
	{ DA_IND32, ECX, 0, 0, -DS },
	{ DA_IND32, EDX, 0, 0, -DS },
	{ DA_IND32, EBX, 0, 0, -DS },
	{ DA_NONE },
	{ DA_IND32, EBP, 0, 0, -SS },
	{ DA_IND32, ESI, 0, 0, -DS },
	{ DA_IND32, EDI, 0, 0, -DS },
};

static xdarg dasib[] = {
	{ DA_SIB, 0, 0, 0, -DS },
	{ DA_SIB, 0, 0, 0, -DS },
	{ DA_SIB, 0, 0, 0, -DS },
};

static xdarg dasibx[] = {
	{ DA_SIBX, 0, 0, 0, -DS },
	{ DA_SIBX, 0, 0, 0, -DS },
	{ DA_SIBX, 0, 0, 0, -DS },
};

static xdarg damem[] = {
	{ DA_MEM, 0, 0, 0, -DS },
	{ DA_MEM, 0, 0, 0, -DS },
	{ DA_MEM, 0, 0, 0, -DS },
};

static xdarg dasegmem[] = {
	{ DA_SEGMEM },
	{ DA_SEGMEM },
	{ DA_SEGMEM },
};

// Instruction decode.

static uint
getimm8(uint8_t **pa)
{
	return *(int8_t*)(*pa)++;
}

static uint
getimm16(uint8_t **pa)
{
	uint8_t *a;
	
	a = *pa;
	*pa += 2;
	return *(int16_t*)a;
}

static uint
getimm32(uint8_t **pa)
{
	uint8_t *a;
	
	a = *pa;
	*pa += 4;
	return *(int32_t*)a;
}

static uint8_t*
decodemodrm(uint8_t *a, uint flags, int sz, uint *reg, xdarg *ea)
{
	int modrm, sib, s, i, b;
	
	memset(ea, 0, sizeof *ea);

	modrm = *a++;
	*reg = (modrm >> 3) & 7;
	if((modrm>>6) == 3){
		*ea = dareg[8*sz + (modrm&7)];
		return a;
	}
	if(!(flags&D_ADDR32)){
		if((modrm>>6) == 0 && (modrm&7) == 6){
			*ea = damem[sz];
			ea->disp = (uint16_t)getimm16(&a);
			return a;
		}
		*ea = daind16[sz*8 + (modrm&7)];
		switch(modrm>>6){
		case 1:
			ea->disp = getimm8(&a);
			break;
		case 2:
			ea->disp = (uint16_t)getimm16(&a);
			break;
		}
	}else{
		if((modrm>>6) == 0 && (modrm&7) == 5){
			*ea = damem[sz];
			ea->disp = getimm32(&a);
			return a;
		}
		switch(modrm&7){
		default:	
			*ea = daind32[sz*8 + (modrm&7)];
			break;
		case 4:
			sib = *a++;
			s = (sib>>6);
			i = (sib>>3)&7;
			b = sib&7;
			*ea = dasib[sz];
			if((modrm>>6) == 0 && b == 5){
				*ea = dasibx[sz];
				ea->disp = getimm32(&a);
			}else{
				*ea = dasib[sz];
				ea->reg = 4*b;
			}
			if(ea->reg == 4*4 || ea->reg == 4*5)
				ea->seg = -SS;
			if(i != 4){
				ea->scale = 1<<s;
				ea->index = 4*i;
			}
			break;
		}
		switch(modrm>>6){
		case 1:
			ea->disp = getimm8(&a);
			break;
		case 2:
			ea->disp = getimm32(&a);
			break;
		}
	}
	return a;
}	

uint8_t*
x86decode(uint8_t *addr0, uint8_t *a, xdinst *dec)
{
	int i, arg, sz, didmodrm;
	uint reg;
	xdarg ea;
	xinst *ip;
	xdarg *da;
	uint8_t *astart;
	int flags;
	
	flags = D_DATA32|D_ADDR32;
	reg = 0;
	
	astart = a;
	dec->addr = a - addr0;

	ip = &inst[*a++];
	
	// xinstruction prefix.
	switch(ip->op){
	case O_LOCK:
		flags |= D_LOCK;
		goto next;
	case O_REPNE:
		flags |= D_REPN;
		goto next;
	case O_REP:
		flags |= D_REP;
	next:
		ip = &inst[*a++];
		break;
	}
	
	// Address size prefix.
	switch(ip->op){
	case O_ADDRSIZE:
		flags ^= D_ADDR32;
		ip = &inst[*a++];
		break;
	}
	
	// Operand size prefix.
	switch(ip->op){
	case O_DATASIZE:
		flags ^= D_DATA16|D_DATA32;
		ip = &inst[*a++];
		break;
	}
	
	// Segment override.
	switch(ip->op){
	case O_SEGMENT:
		flags |= D_ES << (ip->arg1 - A_ES);
		ip = &inst[*a++];
		break;
	}

	if(ip->op == O_NEXTB)
		ip = &ip->sub[*a++];

	didmodrm = 0;
	if(ip->op == O_MODRM){
		ip = &ip->sub[(*a>>3)&7];
		didmodrm = 1;
	}

	switch(ip->op){
	case O_ADDRSIZE:
	case O_DATASIZE:
	case O_SEGMENT:
	case O_LOCK:
	case O_REPNE:
	case O_REP:
	case -1:
		return NULL;
	}
	
	switch(ip->flags){
	case F_X:	// Don't care about operand size; pretend 8-bit for tables.
	case F_8:	// Force 8-bit operands
		flags &= ~(D_DATA16|D_DATA32);
		flags |= D_DATA8;
		break;
	case F_16:	// Force 16-bit operands
		flags &= ~D_DATA32;
		flags |= D_DATA16;
		break;
	case F_32:	// Force 32-bit operands
		flags &= ~D_DATA16;
		flags |= D_DATA32;
	case F_1632:	// Convert 16-bit operands to 32-bit if needed
		break;
	case F_0A:	// Eat 0x0A byte.
		if(*a++ != 0x0A)
			return NULL;
		break;
	}
	if(flags&D_DATA8)
		sz = 0;
	else if(flags&D_DATA16)
		sz = 1;
	else
		sz = 2;

	if(ip->op < 0 || ip->op >= MAXO)
		return NULL;

	dec->name = opnames[ip->op][sz];

	// Mod R/M byte if needed.
	if(didmodrm || needsmodrm[ip->arg1] || needsmodrm[ip->arg2])
		a = decodemodrm(a, flags, sz, &reg, &ea);

	// TO DO: Use override prefixes.

	for(i=0; i<3; i++){
		arg = (&ip->arg1)[i];
		da = &dec->arg[i];
		if(arg == 0){
			da->op = DA_NONE;
			continue;
		}
		if((flags&D_DATA32) && ip->flags == F_1632 && cvt32[arg])
			arg = cvt32[arg];
		if((flags&D_ADDR32) && acvt32[arg])
			arg = acvt32[arg];

		switch(arg){
		case A_0:
			*da = daimm;
			da->disp = 0;
			break;
		case A_1:
		case A_3:
		case A_4:
			*da = daimm;
			da->disp = arg;
			break;
		case A_AL:
		case A_CL:
		case A_DL:
		case A_BL:
		case A_AH:
		case A_CH:
		case A_DH:
		case A_BH:
			*da = dareg[arg - A_AL];
			break;
		case A_AX:
		case A_CX:
		case A_DX:
		case A_BX:
		case A_SP:
		case A_BP:
		case A_SI:
		case A_DI:
			*da = dareg[8 + arg - A_AX];
			break;
		case A_EAX:
		case A_ECX:
		case A_EDX:
		case A_EBX:
		case A_ESP:
		case A_EBP:
		case A_ESI:
		case A_EDI:
			*da = dareg[16 + arg - A_EAX];
			break;
		case A_ES:
		case A_CS:
		case A_SS:
		case A_DS:
		case A_FS:
		case A_GS:
			*da = daseg[arg - A_ES];
			break;

		case A_IMM8:
			*da = daimm;
			da->disp = getimm8(&a);
			break;
		case A_IMM16:
			*da = daimm;
			da->disp = getimm16(&a);
			break;
		case A_IMM32:
			*da = daimm;
			da->disp = getimm32(&a);
			break;

		case A_REL8:
			*da = darel;
			da->disp = getimm8(&a);
			break;
		case A_REL16:
			*da = darel;
			da->disp = getimm16(&a);
			break;
		case A_REL32:
			*da = darel;
			da->disp = getimm32(&a);
			break;

		case A_R8:
		case A_R16:
		case A_R32:
			*da = dareg[sz*8 + reg];
			break;

		case A_RM8:
		case A_RM16:
		case A_RM32:
			*da = ea;
			break;
		
		case A_RM32R:	// A_RM32 but needs to be register
			*da = ea;
			if(da->op > DA_REG32)
				return NULL;
			break;
		
		case A_M:	// A_RM but needs to be memory
			*da = ea;
			if(da->op <= DA_REG32)
				return NULL;
			break;

		case A_MOFF8:
			*da = damem[sz];
			da->disp = getimm8(&a);
			break;
		case A_MOFF16:
			*da = damem[sz];
			da->disp = getimm16(&a);
			break;
		case A_MOFF32:
			*da = damem[sz];
			da->disp = getimm32(&a);
			break;

		case A_SR:
			if(reg > 5)
				return NULL;
			*da = daseg[reg];
			break;

		case A_CR:
			*da = dacr[reg];
			if(da->op == DA_NONE)
				return NULL;
			break;
		case A_DR:
			*da = dadr[reg];
			if(da->op == DA_NONE)
				return NULL;
			break;
		case A_TR:
			*da = dadr[reg];
			if(da->op == DA_NONE)
				return NULL;
			break;

		case A_IMM1616:
		case A_IMM1632:
			*da = dasegmem[sz];
			if(arg == A_IMM1616)
				da->disp = getimm16(&a);
			else
				da->disp = getimm32(&a);
			da->seg = getimm16(&a);
			break;

		}
	}
	dec->opsz = 8<<sz;
	dec->flags = flags;
	dec->len = a - astart;
	if(dec->name == NULL)
		dec->name = "???";
	return a;
}


// Instruction printing

static char*
fmtseg(char *p, char *ep, xdarg *da)
{
	if(da->seg < 0)
		p += snprintf(p, ep-p, "%s:", seg[-da->seg-CS]);
	else
		p += snprintf(p, ep-p, "%#x:", da->seg);
	return p;
}

extern void vxrun_gentrap(void);
extern void vxrun_lookup_backpatch(void);
extern void vxrun_lookup_indirect(void);

static char*
fmtarg(char *p, char *ep, xdarg *da, uint32_t npc)
{
	uint32_t addr;

	switch(da->op){
	default:
		p += snprintf(p, ep-p, "a%d", da->op);
		break;
	case DA_REG8:
		p += snprintf(p, ep-p, "%s", reg8[da->reg/4 + 4*(da->reg%2)]);
		break;
	case DA_REG16:
		p += snprintf(p, ep-p, "%s", reg16[da->reg/4]);
		break;
	case DA_REG32:
		p += snprintf(p, ep-p, "%s", reg32[da->reg/4]);
		break;
	case DA_SEG16:
		p += snprintf(p, ep-p, "%s", seg[da->reg-CS]);
		break;
	case DA_IMM:
		p += snprintf(p, ep-p, "$%#x", da->disp);
		break;
	case DA_REL:
		addr = da->disp + npc;
		if (addr == (uint32_t)(uintptr_t)vxrun_gentrap)
			p += snprintf(p, ep-p, "vxrun_gentrap");
		else if (addr == (uint32_t)(uintptr_t)vxrun_lookup_backpatch)
			p += snprintf(p, ep-p, "vxrun_lookup_backpatch");
		else if (addr == (uint32_t)(uintptr_t)vxrun_lookup_indirect)
			p += snprintf(p, ep-p, "vxrun_lookup_indirect");
		else
			p += snprintf(p, ep-p, "%#x", da->disp + npc);
		break;
	case DA_MEM:
		p = fmtseg(p, ep, da);
		p += snprintf(p, ep-p, "%#x", da->disp);
		break;
	case DA_SEGMEM:
		p += snprintf(p, ep-p, "%#x:%#x", da->seg, da->disp);
		break;
	case DA_IND16:
		p = fmtseg(p, ep, da);
		if(da->disp)
			p += snprintf(p, ep-p, "%#x", da->disp);
		p += snprintf(p, ep-p, "(");
		p += snprintf(p, ep-p, "%s", reg16[da->reg/4]);
		if(da->scale)
			p += snprintf(p, ep-p, "+%s", reg16[da->reg/4]);
		p += snprintf(p, ep-p, ")");
		break;
	case DA_IND32:
		p = fmtseg(p, ep, da);
		if(da->disp)
			p += snprintf(p, ep-p, "%#x", da->disp);
		p += snprintf(p, ep-p, "(%s)", reg32[da->reg/4]);
		break;
	case DA_SIB:
	case DA_SIBX:
		p = fmtseg(p, ep, da);
		if(da->disp)
			p += snprintf(p, ep-p, "%#x", da->disp);
		p += snprintf(p, ep-p, "(");
		if(da->op != DA_SIBX)
			p += snprintf(p, ep-p, "%s+", reg32[da->reg/4]);
		if(da->scale > 1)
			p += snprintf(p, ep-p, "%d*", da->scale);
		p += snprintf(p, ep-p, "%s", reg32[da->index/4]);
		p += snprintf(p, ep-p, ")");
		break;
	}
	return p;
}

int
x86print(char *buf, int nbuf, xdinst *dec)
{
	int i;
	char *p, *ep;
	
	p = buf;
	ep = buf + nbuf;
	for(i=0; i<nelem(prefixes); i++)
		if(dec->flags & prefixes[i].f)
			p += snprintf(p, ep-p, "%s ", prefixes[i].s);
	p += snprintf(p, ep-p, "%-6s", dec->name);
	for(i=0; i<nelem(dec->arg) && dec->arg[i].op != DA_NONE; i++){
		if(i > 0)
			p += snprintf(p, ep-p, ",");
		p += snprintf(p, ep-p, " ");
		p = fmtarg(p, ep, &dec->arg[i], dec->addr + dec->len);
	}
	return p - buf;
}
