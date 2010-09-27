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

char	filebuf[BOOTARGSLEN];

static void
addether(char *name, char *value)
{
	char *p;
	int ctlrno;

	ctlrno = atoi(&name[5]);
	if(ctlrno > MaxEther)
		return;
	p = value;
	while(*p){
		while(*p == ' ' || *p == '\t')
			p++;
		if(*p == '\0')
			break;
		if(strncmp(p, "type=", 5) == 0){
			p += 5;
			if(strncmp(p, "tap", 3) == 0)
				ve[ctlrno].tap = 1;
			else if(strncmp(p, "pcap", 4) == 0)
				ve[ctlrno].tap = 0;
			else
				return;
		}
		else if(strncmp(p, "dev=", 4) == 0){
			p += 4;
			ve[ctlrno].dev = p;
			while(*p && *p != ' ' && *p != '\t')
				p++;
			*p++ = '\0';
			continue;
		}
		else if(strncmp(p, "ea=", 3) == 0){
			if(parseether(ve[ctlrno].ea, p+3) == -1)
				memset(ve[ctlrno].ea, 0, 6);
		}
		while(*p && *p != ' ' && *p != '\t')
			p++;
	}
	nve++;
}

void
setinioptions()
{
	static int i;
	char *name, *value;

	for(; i < MAXCONF; i++){
		if(!inifield[i])
			break;
		name = inifield[i];
		if(*name == '*')
			name++;
		value = strchr(inifield[i], '=');
		if(value == 0)
			continue;
		*value++ = 0;
		if(strcmp(name, "cpulimit") == 0)
			cpulimit = atoi(value);
		else if(strcmp(name, "memsize") == 0)
			memsize = atoi(value);
		else if(strcmp(name, "canopenfiles") == 0)
			canopen = value;
		else if(strncmp(name, "ether", 5) == 0)
			addether(name, value);
		else if(strcmp(name, "initarg") == 0)
			initarg = value;
		else if(strcmp(name, "localroot") == 0)
			localroot = value;
		else if(strcmp(name, "user") == 0)
			username = value;
		/* Restore '=' for setinienv and printconfig */
		*(--value) = '=';
	}
}

void
addini(char *buf)
{
	static int n = 0;
	int blankline, incomment, inspace, inquote;
	char *p, *q;

	/*
	 * Strip out '\r', change '\t' -> ' '.
	 * Change runs of spaces into single spaces.
	 * Strip out trailing spaces, blank lines.
         * The text between single quotes is not touched.
	 *
	 * We do this before we make the copy so that if we 
	 * need to change the copy, it is already fairly clean.
	 * The main need is in the case when plan9.ini has been
	 * padded with lots of trailing spaces, as is the case 
	 * for those created during a distribution install.
	 */
	p = buf;
	blankline = 1;
	incomment = inquote =inspace = 0;
	for(q = buf; *q; q++){
		if(inquote){
			if(*q == '\'')
				inquote = 0;
			*p++ = *q;
			continue;
		}
		if(!incomment && *q == '\'')
			inquote = 1;
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
	*p++ = 0;

	n += gettokens(buf, &inifield[n], MAXCONF-n, "\n");
	setinioptions();
}

int
addinifile(char *file)
{
	static char *fb = filebuf;
	char *buf;
	int n, fd;

	if(strcmp(file, "-") == 0)
		fd = fileno(stdin);
	else if((fd = open(file, OREAD)) < 0)
		return -1;

	buf = fb;
	*buf = 0;
	while((n = read(fd, buf, BOOTARGSLEN-1)) > 0)
		if(n<0)
			return -1;
		else
			buf += n;
	close(fd);
	*buf = 0;
	addini(fb);
	fb = buf;
	return n;
}

char*
fullpath(char *root) {
	char cwd[1024];

	if(root[0] != '/'){
		if(getcwd(cwd, sizeof cwd) == nil)
			panic("getcwd: %r");
		root = cleanname(smprint("%s/%s", cwd, root));
	}
	return root;
}

/*
 * Poor man's quotestrdup to avoid needing quote.c
 */
char*
quoted(char *in) {
	char *out, *p;
	int i, n;

	n = 0;
	for(i = 0; i < strlen(in); i++)
		if(in[i] == '\'')
			n++;
	out = malloc(strlen(in) + n + 2);
	p = out;
	if(*in != '\'')
	*p++ = '\'';
	for(i = 0; i < strlen(in); i++){
		if(in[i] == '\'')
			*p++ = in[i];
		*p++ = in[i];
	}
	*p++ = '\'';
	*p = 0;
	return out;
}

void
setinienv()
{
	int i;
	char *name, *value;

	for(i = 0; i < MAXCONF; i++){
		if(!inifield[i])
			break;
		name = inifield[i];
		value = strchr(inifield[i], '=');
		if(*name == '*' || value == 0 || value[0] == 0)
			continue;
		*value++ = 0;
		ksetenv(name, value, 0);
	}
	if(initarg){
		if(*initarg != '\'')
			initarg = quoted(initarg);
		value = smprint("/386/init -t %s", initarg);
		ksetenv("init", value, 0);
	}
	if(localroot){
		value = smprint("local!#Z%s", fullpath(localroot));
		ksetenv("nobootprompt", value, 0);
	}
	ksetenv("user", username, 0);
}

/*
 * Debugging: tell user what options we guessed.
*/
void
printconfig(char *argv0){
	int i;

	print(argv0);
	if(usetty)
		print(" -%c", nogui ? 'g' : 't');
	print(" \n");
	for(i = 0; i < MAXCONF; i++){
		if(!inifield[i])
			break;
		print("\t%s\n", inifield[i]);
	}
	if(initarg)
		print("\tinit=/386/init -t %s\n", initarg);
	if(localroot)
		print("\tnobootprompt=#Z%s\n", localroot);
	print("\tuser=%s\n", username);
}
