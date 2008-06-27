
int test1()
{
	int n;
	
	asm volatile(
		"movl $0, %%eax\n"
		"movl $20, %%ecx\n"
		"1:\n"
		"incl %%eax\n"
		"loop 1b\n"
		: "=a" (n)
		: "c" (0));

	if(n != 20)
		return 1;
	return 0;
}

int test2()
{
	int n;
	
	asm volatile(
		"movl $20, %%eax\n"
		"movl $20, %%ecx\n"
		"1:\n"
		"decl %%eax\n"
		"loopnz 1b\n"
		: "=a" (n)
		: "c" (0));

	if(n != 0)
		return 1;
	return 0;
}

int test3()
{
	int n;
	
	asm volatile(
		"movl $20, %%eax\n"
		"movl $20, %%ecx\n"
		"1:\n"
		"decl %%eax\n"
		"loopz 1b\n"
		: "=a" (n)
		: "c" (0));

	if(n != 1)
		return 1;
	return 0;
}

int
main(void)
{
	return test1() ||
		test2() ||
		test3();
}
