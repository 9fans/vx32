//
// Definitions used within VX environments
// to invoke VX system calls and parent calls.
//
#ifndef _VXSYS_H_
#define _VXSYS_H_


// VX parent-calls take the parent-call code and flags in EAX,
// If the VXPC_SENDBUF flag is set, EBX points to a message to send
// and EDX contains the message's length in bytes.
// The ECX register cannot be used for call or return arguments,
// as it is used as a temporary by the SYSCALL instruction.
// All other registers are simple register arguments to the parent.
#define VXPCALL(a, s1, s2, r1, r2) \
	asm volatile("syscall" \
		: "=a" (a), "=b" (s1), "=d" (s2) \
		: "a" (a), "b" (s1), "d" (s2), "S" (r1), "D" (r2) \
		: "ecx");


static inline void vxsetperm(void *addr, size_t size, int perm)
{
	asm volatile("syscall"
		: 
		: "a" (VXSYS_SETPERM), "b" (addr), "d" (size), "S" (perm)
		: "ecx");
}

#endif	// _VXSYS_H_
