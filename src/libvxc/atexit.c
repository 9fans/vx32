
#include <stdlib.h>
#include <unistd.h>

#include "private.h"


typedef void exitfunc(void);

#define MAXEXITFUNCS	32	// POSIX's standard minimum

static exitfunc *exitfuncs[MAXEXITFUNCS];
static int nexitfuncs;


static void callfuncs()
{
	// Call all exit handlers
	for (int i = nexitfuncs-1; i >= 0; i--)
		exitfuncs[i]();
}

int atexit(exitfunc *func)
{
	if (nexitfuncs == MAXEXITFUNCS)
		return -1;

	__exit_atexit = callfuncs;
	exitfuncs[nexitfuncs++] = func;
	return 0;
}

