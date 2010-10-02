#include <stdio.h>
#include <string.h>
#include <errno.h>

void perror(const char *s)
{
	char *e = strerror(errno);
	if (s)
		fprintf(stderr, "%s: %s\n", s, e);
	else
		fprintf(stderr, "%s\n", e);
}
