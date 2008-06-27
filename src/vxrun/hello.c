#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

int 
main(int argc, char **argv)
{
	int i;
	unsigned long a, b, c;
	
	printf("argc %d argv %p\n", argc, argv);
	a = 1;
	b = 2;
	c = 3;
	write(1, "hello\n", 6);
	printf("%8ld %8ld %8ld\n", a, b, c);
	
	double d;
	sscanf("30.0", "%lf", &d);
	printf("%g\n", d);
	printf("%g\n", strtod("30.0", NULL));

	printf("%d args\n", argc);
	for(i=0; i<argc; i++){
		printf("arg%d: %s\n", i, argv[i]);
	}
	extern char **environ;
	for(i=0; environ[i]; i++){
		printf("env%d: %s\n", i, environ[i]);
	}

	int x;
	__asm("fnstcw %0" : "=m" (x));
	printf("float %x\n", x);
	return 0;
}

