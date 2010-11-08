
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

void __assert_fail(const char *file, int line,
			const char *func, const char *exp)
{
	fprintf(stderr, "%s:%d: failed assertion '%s' in function %s\n",
		file, line, exp, func);
	abort();
}

