#include "u.h"
#include "lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "io.h"

#include "fs.h"

/*
 * Where configuration info is left for the loaded programme.
 * This will turn into a structure as more is done by the boot loader
 * (e.g. why parse the .ini file twice?).
 * There are 3584 bytes available at CONFADDR.
 *
 * The low-level boot routines in l.s leave data for us at CONFADDR,
 * which we pick up before reading the plan9.ini file.
 */
#define BOOTLINELEN	64
#define	BOOTARGSLEN	(3584-0x200-BOOTLINELEN)
#define	MAXCONF		100

extern char **ini;

typedef struct {
	char*	name;
	int	start;
	int	end;
} Mblock;

typedef struct {
	char*	tag;
	Mblock*	mb;
} Mitem;

static Mblock mblock[MAXCONF];
static int nmblock;
static Mitem mitem[MAXCONF];
static int nmitem;
static char* mdefault;
static char mdefaultbuf[10];
static int mtimeout;

static void
getstr(char* prompt, char* buf, int n, char* def, int timeout)
{
	char *p;
	int i;
	char c;

	if(def == nil)
		print("%s: ", prompt);
	else
		print("%s[default==%s]: ", prompt, def);
	for(p = buf, i = 0; i < n-1;){
		qread(kbdq, &c, 1);
		switch(c){
		case '\b':
			if(i > 0){
				--p;
				--i;
			}
			break;
		case 0x15:
			p = buf;
			i = 0;
			break;
		case '\n':
			break;
		default:
			*p++ = c;
			++i;
			break;
		}
		if(c == '\n')
			break;
	}
	*p = 0;
	if(i == 0)
		strcpy(buf, def);
}

static char*
comma(char* line, char** residue)
{
	char *q, *r;

	if((q = strchr(line, ',')) != nil){
		*q++ = 0;
		if(*q == ' ')
			q++;
	}
	*residue = q;

	if((r = strchr(line, ' ')) != nil)
		*r = 0;

	if(*line == ' ')
		line++;
	return line;
}

static Mblock*
findblock(char* name, char** residue)
{
	int i;
	char *p;

	p = comma(name, residue);
	for(i = 0; i < nmblock; i++){
		if(strcmp(p, mblock[i].name) == 0)
			return &mblock[i];
	}
	return nil;
}

static Mitem*
finditem(char* name, char** residue)
{
	int i;
	char *p;

	p = comma(name, residue);
	for(i = 0; i < nmitem; i++){
		if(strcmp(p, mitem[i].mb->name) == 0)
			return &mitem[i];
	}
	return nil;
}

static void
parsemenu(char* str, int len)
{
	Mitem *mi;
	Mblock *mb, *menu;
	char buf[20], scratch[BOOTARGSLEN], *p, *q, *line[MAXCONF];
	int i, inblock, n, show;

	inblock = 0;
	menu = nil;
	memmove(scratch, str, len);
	n = getfields(scratch, line, MAXCONF, 0, "\n");
	if(n >= MAXCONF)
		print("warning: possibly too many lines in plan9.ini\n");
	for(i = 0; i < n; i++){
		p = line[i];
		if(inblock && *p == '['){
			mblock[nmblock].end = i;
			if(strcmp(mblock[nmblock].name, "menu") == 0)
				menu = &mblock[nmblock];
			nmblock++;
			inblock = 0;
		}
		if(*p == '['){
			if(nmblock == 0 && i != 0){
				mblock[nmblock].name = "common";
				mblock[nmblock].start = 0;
				mblock[nmblock].end = i;
				nmblock++;
			}
			q = strchr(p+1, ']');
			if(q == nil || *(q+1) != 0){
				print("malformed menu block header - %s\n", p);
				return;
			}
			*q = 0;
			mblock[nmblock].name = p+1;
			mblock[nmblock].start = i+1;
			inblock = 1;
		}
	}

	if(inblock){
		mblock[nmblock].end = i;
		nmblock++;
	}
	if(menu == nil)
		return;
	if(nmblock < 2){
		print("incomplete menu specification\n");
		return;
	}

	for(i = menu->start; i < menu->end; i++){
		p = line[i];
		if(strncmp(p, "menuitem=", 9) == 0){
			p += 9;
			if((mb = findblock(p, &q)) == nil){
				print("no block for menuitem %s\n", p);
				return;
			}
			if(q != nil)
				mitem[nmitem].tag = q;
			else
				mitem[nmitem].tag = mb->name;
			mitem[nmitem].mb = mb;
			nmitem++;
		}
		else if(strncmp(p, "menudefault=", 12) == 0){
			p += 12;
			if((mi = finditem(p, &q)) == nil){
				print("no item for menudefault %s\n", p);
				return;
			}
			if(q != nil)
				mtimeout = strtol(q, 0, 0);
			sprint(mdefaultbuf, "%ld", mi-mitem+1);
			mdefault = mdefaultbuf;
		}
		else if(strncmp(p, "menuconsole=", 12) == 0){
			p += 12;
			p = comma(p, &q);
			/* consinit(p, q); */
		}
		else{
			print("invalid line in [menu] block - %s\n", p);
			return;
		}
	}

again:
	print("\nPlan 9 Startup Menu:\n====================\n");
	for(i = 0; i < nmitem; i++)
		print("    %d. %s\n", i+1, mitem[i].tag);
	for(;;){
		getstr("Selection", buf, sizeof(buf), mdefault, mtimeout);
		mtimeout = 0;
		i = strtol(buf, &p, 0)-1;
		if(i < 0 || i >= nmitem)
			goto again;
		switch(*p){
		case 'p':
		case 'P':
			show = 1;
			print("\n");
			break;
		case 0:
			show = 0;
			break;
		default:
			continue;
			
		}
		mi = &mitem[i];
	
		p = str;
		p += sprint(p, "menuitem=%s\n", mi->mb->name);
		for(i = 0; i < nmblock; i++){
			mb = &mblock[i];
			if(mi->mb != mb && strcmp(mb->name, "common") != 0)
				continue;
			for(n = mb->start; n < mb->end; n++)
				p += sprint(p, "%s\n", line[n]);
		}

		if(show){
			for(q = str; q < p; q += i){
				if((i = print(q)) <= 0)
					break;
			}
			goto again;
		}
		break;
	}
	print("\n");
}

/*
 *  read configuration file
 */
static char inibuf[BOOTARGSLEN];

int
dotini(char *fn)
{
	int blankline, i, incomment, inspace, n, fd;
	char *cp, *p, *q, *line[MAXCONF];

	if((fd = open(fn, OREAD)) < 0)
		return -1;

	cp = inibuf;
	*cp = 0;
	n = read(fd, cp, BOOTARGSLEN-1);
	close(fd);
	if(n <= 0)
		return -1;

	cp[n] = 0;

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
	n = p-cp;

	parsemenu(cp, n);

	n = getfields(cp, line, MAXCONF, 0, "\n");
	for(i = 0; i < n; i++){
		cp = strchr(line[i], '=');
		if(cp == 0)
			continue;
		*cp++ = 0;
		if(cp - line[i] >= NAMELEN+1)
			*(line[i]+NAMELEN-1) = 0;
		ksetenv(line[i], cp, 0);
	}
	return 0;
}
