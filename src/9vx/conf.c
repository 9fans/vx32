#include	"u.h"
#include	"lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"

#include	"fs.h"

#include	"conf.h"

#include	"netif.h"
#include	"etherif.h"
#include 	"vether.h"

/*
 *  read configuration file
 */
int
readini(char *fn)
{
	int blankline, incomment, inspace, n, fd;
	static int nfields = 0;
	static char *buf = inibuf;
	char *cp, *p, *q;

	if(strcmp(fn, "-") == 0)
		fd = fileno(stdin);
	else if((fd = open(fn, OREAD)) < 0)
		return -1;

	cp = buf;
	*buf = 0;
	while((n = read(fd, buf, BOOTARGSLEN-1)) > 0)
		if(n<0)
			return -1;
		else
			buf += n;
	close(fd);
	*buf = 0;

	/*
	 * Strip out '\r', change '\t' -> ' '.
	 * Change runs of spaces into single spaces.
	 * Strip out trailing spaces, blank lines.
	 *
	 * We do this before we make the copy so that if we 
	 * need to change the copy, it is already fairly clean.
	 * The main need is in the case when plan9.ini has been
	 * padded with lots of trailing spaces, as is the case 
	 * for those created during a distribution install.
	 */
	p = cp;
	blankline = 1;
	incomment = inspace = 0;
	for(q = cp; *q; q++){
		if(*q == '\r')
			continue;
		if(*q == '\t')
			*q = ' ';
		if(*q == ' '){
			inspace = 1;
			continue;
		}
		if(*q == '\n'){
			if(!blankline){
				if(!incomment)
					*p++ = '\n';
				blankline = 1;
			}
			incomment = inspace = 0;
			continue;
		}
		if(inspace){
			if(!blankline && !incomment)
				*p++ = ' ';
			inspace = 0;
		}
		if(blankline && *q == '#')
			incomment = 1;
		blankline = 0;
		if(!incomment)
			*p++ = *q;	
	}
	if(p > cp && p[-1] != '\n')
		*p++ = '\n';
	*p++ = 0;

	nfields += getfields(cp, &iniline[nfields], MAXCONF-nfields, 0, "\n");

	return 0;
}

void
inifields(void (*fp)(char*, char*))
{
	int i;
	char *cp;

	for(i = 0; i < MAXCONF; i++){
		if(!iniline[i])
			break;
		cp = strchr(iniline[i], '=');
		if(cp == 0)
			continue;
		*cp++ = 0;
		if(cp - iniline[i] >= NAMELEN+1)
			*(iniline[i]+NAMELEN-1) = 0;
		(fp)(iniline[i], cp);
		*(cp-1) = '=';
	}
}

void
iniopt(char *name, char *value)
{
	char *cp, *vedev;
	int vetap;

	if(*name == '*')
		name++;
	if(strcmp(name, "nofork") == 0)
		nofork = 1;
	else if(strcmp(name, "nogui") == 0){
		nogui = 1;
		usetty = 1;
	}
	else if(strcmp(name, "initrc") == 0)
		initrc = 1;
	else if(strcmp(name, "usetty") == 0)
		usetty = 1;
	else if(strcmp(name, "cpulimit") == 0)
		cpulimit = atoi(value);
	else if(strcmp(name, "memsize") == 0)
		memmb = atoi(value);
	else if(strcmp(name, "netdev") == 0){
		if(strncmp(value, "tap", 3) == 0) {
			vetap = 1;
			value += 4;
		}
		vedev = value;
		cp = vedev;
		if((value = strchr(vedev, ' ')) != 0){
			cp = strchr(value+1, '=');
			*value=0;
			*cp=0;
		}
		addve(*vedev == 0 ? nil : vedev, vetap);
		if(cp != vedev){
			iniopt(value+1, cp+1);
			*value=' ';
			*cp='=';
		}
	}
	else if(strcmp(name, "macaddr") == 0)
		setmac(value);
	else if(strcmp(name, "localroot") == 0 && !localroot)
		localroot = value;
	else if(strcmp(name, "user") == 0 && !username)
		username = value;
}

void
inienv(char *name, char *value)
{
	if(*name != '*')
		ksetenv(name, value, 0);
}

/*
 * Debugging: tell user what options we guessed.
*/
void
printconfig(char *argv0){
	int i;

	print(argv0);
	if(inifile)
		print(" -p %s", inifile);
	if(cpuserver | nofork | nogui | initrc | usetty)
		print(" -%s%s%s%s%s", cpuserver ? "c " : "", nofork ? "f " : "",
			nogui ? "g" : "",initrc ? "i " : "", usetty ? "t " : "");
	if(cpulimit != 0)
		print(" -l %d", cpulimit);
	if(memmb != 0)
		print(" -m %d", memmb);
	for(i=0; i<nve; i++){
		print(" -n %s", ve[i].tap ? "tap ": "");
		if(ve[i].dev != nil)
			print(" %s", ve[i].dev);
		if(ve[i].mac != nil)
			print(" -a %s", ve[i].mac);
	}
	if(localroot)
		print(" -r %s", localroot);
	print(" -u %s", username);
	for(i = 0; i < bootargc; i++)
		print(" %s", bootargv[i]);
	print("\n");
}