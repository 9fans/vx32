/* 
 * New file, replaces the Plan 9 version of data2s.
 * This one emits GNU assembler syntax.
 */

#include <stdio.h>
#include <stdlib.h>

#ifdef __APPLE__
#define prefix "_"	/* go back in time */
#else
#define prefix ""
#endif

int
main(int argc, char *argv[])
{
	long len, slen;
	int c;

	if(argc != 2){
		fprintf(stderr, "usage: data2s name\n");
		exit(1);
	}
	printf(".data\n");
	printf(".globl %s%scode\n", prefix, argv[1]);
	printf(".globl %s%slen\n", prefix, argv[1]);
	printf("%s%scode:\n", prefix, argv[1]);
	for(len=0; (c=fgetc(stdin))!=EOF; len++){
		if((len&7) == 0)
			printf(".byte");
		else
			printf(",");
		printf(" %#x", c&0xff);
		if((len&7) == 7)
			printf("\n");
	}
	printf("\n\n.p2align 2\n%s%slen:\n", prefix, argv[1]);
	printf(".long %d\n", len);
	return 0;
}
