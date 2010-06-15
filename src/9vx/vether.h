typedef struct Vether Vether;
struct Vether
{
	int	tap;
	char	*dev;
	uchar ea[Eaddrlen];
};

static Vether ve[MaxEther+1];
static int nve = 0;
