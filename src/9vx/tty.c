/*
 * Terminal support (standard input/output).
 */

#include	"u.h"
#include	<termios.h>
#include	<sys/termios.h>
#include	"lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"error.h"

static int ttyprint = 0;
static int ttyecho = 0;

/*
 * Normal prints and console output go to standard output.
 */
void
uartputs(char *buf, int n)
{
	if(!ttyprint)
		return;
	write(1, buf, n);
}

void
uartreader(void *v)
{
	char buf[256];
	int n;
	static struct termios ttmode;
	
	/*
	 * Try to disable host echo. 
	 * If successful, remember to echo
	 * what gets typed ourselves.
	 */
	if(tcgetattr(0, &ttmode) >= 0){
		ttmode.c_lflag &= ~(ECHO|ICANON);
		if(tcsetattr(0, TCSANOW, &ttmode) >= 0)
			ttyecho = 1;
	}
	while((n = read(0, buf, sizeof buf)) > 0)
		echo(buf, n);
}

void
uartinit(int usetty)
{
	ttyprint = usetty;
	kbdq = qopen(4*1024, 0, 0, 0);
	if(usetty)
		kproc("*tty*", uartreader, nil);
}

void
uartecho(char *buf, int n)
{
	if(ttyecho)
		write(1, buf, n);
}

