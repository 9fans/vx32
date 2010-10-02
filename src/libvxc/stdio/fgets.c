#include <stdio.h>

char *fgets(char *s, int size, FILE *f)
{
	char *p = s;
	char *ep = s + size;

	if (size < 1)
		return NULL;

	while (p < ep-1) {
		int c = fgetc(f);
		if (c == EOF || c == '\n')
			break;
		*p++ = c;
	}
	*p = 0;
	return s;
}

