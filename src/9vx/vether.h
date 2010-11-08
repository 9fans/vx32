typedef struct Vether Vether;
struct Vether
{
	int	tap;
	char	*dev;
	uchar ea[Eaddrlen];
};

Vether ve[MaxEther+1];
int nve;

void	links();
