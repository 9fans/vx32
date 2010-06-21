typedef struct Vether Vether;
struct Vether
{
	int	tap;
	char	*dev;
	char	*mac;
	uchar ea[Eaddrlen];
};

Vether ve[MaxEther+1];
int nve;

void	setmac(char*);
void	addve(char*, int);
void	links();
