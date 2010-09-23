#define	BOOTLINELEN	64
#define	BOOTARGSLEN	(3584-0x200-BOOTLINELEN)
#define	MAXCONF		100

char	*inifield[MAXCONF];
int	nofork;
int	nogui;
int	usetty;
int	cpulimit;
int	memsize;
int	bootargc;
char**	bootargv;
char*	canopen;
char*	initarg;
char*	localroot;
char*	username;

int	addinifile(char*);
void	addini(char*);
void	printconfig(char*);
void	setinienv();
