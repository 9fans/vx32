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

void	setea(char*);
void	addve(char*, int);
void	links();
