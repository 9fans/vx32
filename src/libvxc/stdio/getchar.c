
#include <stdio.h>

#undef getchar
int getchar()
{
	return fgetc(stdin);
}

