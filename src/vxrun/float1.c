#include <stdio.h>

int Prob2Score(float, float);

int main(int argc, char **argv)
{
	float f;
	f = 10.0;

	int i;
	for(i=0; i<4; i++)
		Prob2Score(1, 1);
	printf("%f\n", f);
	return 0;
}
