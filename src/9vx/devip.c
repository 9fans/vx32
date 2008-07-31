/*
 * /net interface to host IPv4 stack.
 */

#include "u.h"
#include "lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "error.h"
#include "ip.h"
#include "devip.h"

void	csclose(Chan*);
long	csread(Chan*, void*, long, vlong);
long	cswrite(Chan*, void*, long, vlong);

static int csremoved = 1;	/* was nice while it lasted... */

void osipinit(void);

enum
{
	Qtopdir		= 1,	/* top level directory */
	Qcs,
	Qdns,
	Qprotodir,		/* directory for a protocol */
	Qclonus,
	Qconvdir,		/* directory for a conversation */
	Qdata,
	Qctl,
	Qstatus,
	Qremote,
	Qlocal,
	Qlisten,

	MAXPROTO	= 4
};
#define TYPE(x) 	((int)((x).path & 0xf))
#define CONV(x) 	((int)(((x).path >> 4)&0xfff))
#define PROTO(x) 	((int)(((x).path >> 16)&0xff))
#define QID(p, c, y) 	(((p)<<16) | ((c)<<4) | (y))

typedef struct Proto	Proto;
typedef struct Conv	Conv;
struct Conv
{
	int	x;
	Ref	r;
	int	sfd;
	int	eof;
	int	perm;
	char	owner[KNAMELEN];
	char*	state;
	ulong	laddr;
	ushort	lport;
	ulong	raddr;
	ushort	rport;
	int	restricted;
	char	cerr[KNAMELEN];
	Proto*	p;
};

struct Proto
{
	Lock	l;
	int	x;
	int	stype;
	char	name[KNAMELEN];
	int	nc;
	int	maxconv;
	Conv**	conv;
	Qid	qid;
};

static	int	np;
static	Proto	proto[MAXPROTO];

static	Conv*	protoclone(Proto*, char*, int);
static	void	setladdr(Conv*);

int
ipgen(Chan *c, char *nname, Dirtab *d, int nd, int s, Dir *dp)
{
	Qid q;
	Conv *cv;
	char *p;

	USED(nname);
	q.vers = 0;
	q.type = 0;
	switch(TYPE(c->qid)) {
	case Qtopdir:
	case Qcs:
	case Qdns:
		if(s >= 2+np)
			return -1;
		if(s == 0){
			if(csremoved)
				return 0;
			q.path = QID(s, 0, Qcs);
			devdir(c, q, "cs", 0, "network", 0666, dp);
		}else if(s == 1){
			q.path = QID(s, 0, Qdns);
			devdir(c, q, "dns", 0, "network", 0666, dp);
		}else{
			s-=2;
			q.path = QID(s, 0, Qprotodir);
			q.type = QTDIR;
			devdir(c, q, proto[s].name, 0, "network", DMDIR|0555, dp);
		}
		return 1;
	case Qprotodir:
	case Qclonus:
		if(s < proto[PROTO(c->qid)].nc) {
			cv = proto[PROTO(c->qid)].conv[s];
			sprint(up->genbuf, "%d", s);
			q.path = QID(PROTO(c->qid), s, Qconvdir);
			q.type = QTDIR;
			devdir(c, q, up->genbuf, 0, cv->owner, DMDIR|0555, dp);
			return 1;
		}
		s -= proto[PROTO(c->qid)].nc;
		switch(s) {
		default:
			return -1;
		case 0:
			p = "clone";
			q.path = QID(PROTO(c->qid), 0, Qclonus);
			break;
		}
		devdir(c, q, p, 0, "network", 0555, dp);
		return 1;
	case Qconvdir:
	case Qdata:
	case Qctl:
	case Qstatus:
	case Qremote:
	case Qlocal:
	case Qlisten:
		cv = proto[PROTO(c->qid)].conv[CONV(c->qid)];
		switch(s) {
		default:
			return -1;
		case 0:
			q.path = QID(PROTO(c->qid), CONV(c->qid), Qdata);
			devdir(c, q, "data", 0, cv->owner, cv->perm, dp);
			return 1;
		case 1:
			q.path = QID(PROTO(c->qid), CONV(c->qid), Qctl);
			devdir(c, q, "ctl", 0, cv->owner, cv->perm, dp);
			return 1;
		case 2:
			p = "status";
			q.path = QID(PROTO(c->qid), CONV(c->qid), Qstatus);
			break;
		case 3:
			p = "remote";
			q.path = QID(PROTO(c->qid), CONV(c->qid), Qremote);
			break;
		case 4:
			p = "local";
			q.path = QID(PROTO(c->qid), CONV(c->qid), Qlocal);
			break;
		case 5:
			p = "listen";
			q.path = QID(PROTO(c->qid), CONV(c->qid), Qlisten);
			break;
		}
		devdir(c, q, p, 0, cv->owner, 0444, dp);
		return 1;
	}
	return -1;
}

static void
newproto(char *name, int type, int maxconv)
{
	int l;
	Proto *p;

	if(np >= MAXPROTO) {
		print("no %s: increase MAXPROTO", name);
		return;
	}

	p = &proto[np];
	strcpy(p->name, name);
	p->stype = type;
	p->qid.path = QID(np, 0, Qprotodir);
	p->qid.type = QTDIR;
	p->x = np++;
	p->maxconv = maxconv;
	l = sizeof(Conv*)*(p->maxconv+1);
	p->conv = mallocz(l, 1);
	if(p->conv == 0)
		panic("no memory");
}

void
ipinit(void)
{
	osipinit();

	newproto("udp", S_UDP, 10);
	newproto("tcp", S_TCP, 30);

	fmtinstall('E', eipfmt);
	fmtinstall('V', eipfmt);
}

Chan *
ipattach(char *spec)
{
	Chan *c;

	c = devattach('I', spec);
	c->qid.path = QID(0, 0, Qtopdir);
	c->qid.type = QTDIR;
	c->qid.vers = 0;
	return c;
}

static Walkqid*
ipwalk(Chan *c, Chan *nc, char **name, int nname)
{
	return devwalk(c, nc, name, nname, 0, 0, ipgen);
}

int
ipstat(Chan *c, uchar *dp, int n)
{
	return devstat(c, dp, n, 0, 0, ipgen);
}

Chan *
ipopen(Chan *c, int omode)
{
	Proto *p;
	ulong raddr;
	ushort rport;
	int perm, sfd;
	Conv *cv, *lcv;

	omode &= 3;
	perm = 0;
	switch(omode) {
	case OREAD:
		perm = 4;
		break;
	case OWRITE:
		perm = 2;
		break;
	case ORDWR:
		perm = 6;
		break;
	}

	switch(TYPE(c->qid)) {
	default:
		break;
	case Qtopdir:
	case Qprotodir:
	case Qconvdir:
	case Qstatus:
	case Qremote:
	case Qlocal:
		if(omode != OREAD)
			error(Eperm);
		break;
	case Qclonus:
		p = &proto[PROTO(c->qid)];
		cv = protoclone(p, up->user, -1);
		if(cv == 0)
			error(Enodev);
		c->qid.path = QID(p->x, cv->x, Qctl);
		c->qid.vers = 0;
		break;
	case Qdata:
	case Qctl:
		p = &proto[PROTO(c->qid)];
		lock(&p->l);
		cv = p->conv[CONV(c->qid)];
		lock(&cv->r.lk);
		if((perm & (cv->perm>>6)) != perm) {
			if(strcmp(up->user, cv->owner) != 0 ||
		 	  (perm & cv->perm) != perm) {
				unlock(&cv->r.lk);
				unlock(&p->l);
				error(Eperm);
			}
		}
		cv->r.ref++;
		if(cv->r.ref == 1) {
			memmove(cv->owner, up->user, KNAMELEN);
			cv->perm = 0660;
		}
		unlock(&cv->r.lk);
		unlock(&p->l);
		break;
	case Qlisten:
		p = &proto[PROTO(c->qid)];
		lcv = p->conv[CONV(c->qid)];
		sfd = so_accept(lcv->sfd, &raddr, &rport);
		cv = protoclone(p, up->user, sfd);
		if(cv == 0) {
			close(sfd);
			error(Enodev);
		}
		cv->raddr = raddr;
		cv->rport = rport;
		setladdr(cv);
		cv->state = "Established";
		c->qid.path = QID(p->x, cv->x, Qctl);
		break;
	}
	c->mode = openmode(omode);
	c->flag |= COPEN;
	c->offset = 0;
	return c;
}

void
ipclose(Chan *c)
{
	Conv *cc;

	switch(TYPE(c->qid)) {
	case Qcs:
	case Qdns:
		csclose(c);
		break;
	case Qdata:
	case Qctl:
		if((c->flag & COPEN) == 0)
			break;
		cc = proto[PROTO(c->qid)].conv[CONV(c->qid)];
		if(decref(&cc->r) != 0)
			break;
		strcpy(cc->owner, "network");
		cc->perm = 0666;
		cc->state = "Closed";
		cc->laddr = 0;
		cc->raddr = 0;
		cc->lport = 0;
		cc->rport = 0;
		close(cc->sfd);
		break;
	}
}

void
ipremove(Chan *c)
{
	if(TYPE(c->qid) == Qcs){
		csremoved = 1;
		csclose(c);
		return;
	}
	devremove(c);
}

long
ipread(Chan *ch, void *a, long n, vlong offset)
{
	int r;
	Conv *c;
	Proto *x;
	uchar ip[4];
	char buf[128], *p;

/*print("ipread %s %lux\n", c2name(ch), (long)ch->qid.path);*/
	p = a;
	switch(TYPE(ch->qid)) {
	default:
		error(Eperm);
	case Qcs:
	case Qdns:
		return csread(ch, a, n, offset);
	case Qprotodir:
	case Qtopdir:
	case Qconvdir:
		return devdirread(ch, a, n, 0, 0, ipgen);
	case Qctl:
		sprint(buf, "%d", CONV(ch->qid));
		return readstr(offset, p, n, buf);
	case Qremote:
		c = proto[PROTO(ch->qid)].conv[CONV(ch->qid)];
		hnputl(ip, c->raddr);
		sprint(buf, "%V!%d\n", ip, c->rport);
		return readstr(offset, p, n, buf);
	case Qlocal:
		c = proto[PROTO(ch->qid)].conv[CONV(ch->qid)];
		hnputl(ip, c->laddr);
		sprint(buf, "%V!%d\n", ip, c->lport);
		return readstr(offset, p, n, buf);
	case Qstatus:
		x = &proto[PROTO(ch->qid)];
		c = x->conv[CONV(ch->qid)];
		sprint(buf, "%s/%d %d %s \n",
			c->p->name, c->x, c->r.ref, c->state);
		return readstr(offset, p, n, buf);
	case Qdata:
		c = proto[PROTO(ch->qid)].conv[CONV(ch->qid)];
		r = so_recv(c->sfd, a, n, 0);
		if(r < 0){
			oserrstr();
			nexterror();
		}
		if(r == 0 && ++c->eof > 3)
			error(Ehungup);
		return r;
	}
}

static void
setladdr(Conv *c)
{
	so_getsockname(c->sfd, &c->laddr, &c->lport);
}

static void
setlport(Conv *c)
{
	if(c->restricted == 0 && c->lport == 0)
		return;

	so_bind(c->sfd, c->restricted, c->lport);
}

static void
setladdrport(Conv *c, char *str)
{
	char *p;
	uchar addr[4];

	p = strchr(str, '!');
	if(p == 0) {
		p = str;
		c->laddr = 0;
	}
	else {
		*p++ = 0;
		v4parseip(addr, str);
		c->laddr = nhgetl(addr);
	}
	if(*p == '*')
		c->lport = 0;
	else
		c->lport = atoi(p);

	setlport(c);
}

static char*
setraddrport(Conv *c, char *str)
{
	char *p;
	uchar addr[4];

	p = strchr(str, '!');
	if(p == 0)
		return "malformed address";
	*p++ = 0;
	v4parseip(addr, str);
	c->raddr = nhgetl(addr);
	c->rport = atoi(p);
	p = strchr(p, '!');
	if(p) {
		if(strcmp(p, "!r") == 0)
			c->restricted = 1;
	}
	return 0;
}

long
ipwrite(Chan *ch, void *a, long n, vlong offset)
{
	Conv *c;
	Proto *x;
	int r, nf;
	char *p, *fields[3], buf[128];

	switch(TYPE(ch->qid)) {
	default:
		error(Eperm);
	case Qcs:
	case Qdns:
		return cswrite(ch, a, n, offset);
	case Qctl:
		x = &proto[PROTO(ch->qid)];
		c = x->conv[CONV(ch->qid)];
		if(n > sizeof(buf)-1)
			n = sizeof(buf)-1;
		memmove(buf, a, n);
		buf[n] = '\0';

		nf = tokenize(buf, fields, 3);
		if(strcmp(fields[0], "connect") == 0){
			switch(nf) {
			default:
				error("bad args to connect");
			case 2:
				p = setraddrport(c, fields[1]);
				if(p != 0)
					error(p);
				break;
			case 3:
				p = setraddrport(c, fields[1]);
				if(p != 0)
					error(p);
				c->lport = atoi(fields[2]);
				setlport(c);
				break;
			}
			so_connect(c->sfd, c->raddr, c->rport);
			setladdr(c);
			c->state = "Established";
			return n;
		}
		if(strcmp(fields[0], "announce") == 0) {
			switch(nf){
			default:
				error("bad args to announce");
			case 2:
				setladdrport(c, fields[1]);
				break;
			}
			so_listen(c->sfd);
			c->state = "Announced";
			return n;
		}
		if(strcmp(fields[0], "bind") == 0){
			switch(nf){
			default:
				error("bad args to bind");
			case 2:
				c->lport = atoi(fields[1]);
				break;
			}
			setlport(c);
			return n;
		}
		error("bad control message");
	case Qdata:
		x = &proto[PROTO(ch->qid)];
		c = x->conv[CONV(ch->qid)];
		r = so_send(c->sfd, a, n, 0);
		if(r < 0){
			oserrstr();
			nexterror();
		}
		return r;
	}
	return n;
}

static Conv*
protoclone(Proto *p, char *user, int nfd)
{
	Conv *c, **pp, **ep;

	c = 0;
	lock(&p->l);
	if(waserror()) {
		unlock(&p->l);
		nexterror();
	}
	ep = &p->conv[p->maxconv];
	for(pp = p->conv; pp < ep; pp++) {
		c = *pp;
		if(c == 0) {
			c = mallocz(sizeof(Conv), 1);
			if(c == 0)
				error(Enomem);
			lock(&c->r.lk);
			c->r.ref = 1;
			c->p = p;
			c->x = pp - p->conv;
			p->nc++;
			*pp = c;
			break;
		}
		lock(&c->r.lk);
		if(c->r.ref == 0) {
			c->r.ref++;
			break;
		}
		unlock(&c->r.lk);
	}
	if(pp >= ep) {
		unlock(&p->l);
		poperror();
		return 0;
	}

	strcpy(c->owner, user);
	c->perm = 0660;
	c->state = "Closed";
	c->restricted = 0;
	c->laddr = 0;
	c->raddr = 0;
	c->lport = 0;
	c->rport = 0;
	c->sfd = nfd;
	if(nfd == -1)
		c->sfd = so_socket(p->stype);
	c->eof = 0;

	unlock(&c->r.lk);
	unlock(&p->l);
	poperror();
	return c;
}

/*
 * In-kernel /net/cs and /net/dns
 */
void
csclose(Chan *c)
{
	free(c->aux);
}

long
csread(Chan *c, void *a, long n, vlong offset)
{
	if(c->aux == nil)
		return 0;
	return readstr(offset, a, n, c->aux);
}

static struct
{
	char *proto;
	char *name;
	uint num;
} tab[] = {
	"tcp", "cs", 1,
	"tcp", "echo", 7,
	"tcp", "discard", 9,
	"tcp", "systat", 11,
	"tcp", "daytime", 13,
	"tcp", "netstat", 15,
	"tcp", "chargen", 19,
	"tcp", "ftp-data", 20,
	"tcp", "ftp", 21,
	"tcp", "ssh", 22,
	"tcp", "telnet", 23,
	"tcp", "smtp", 25,
	"tcp", "time", 37,
	"tcp", "whois", 43,
	"tcp", "dns", 53,
	"tcp", "domain", 53,
	"tcp", "uucp", 64,
	"tcp", "gopher", 70,
	"tcp", "rje", 77,
	"tcp", "finger", 79,
	"tcp", "http", 80,
	"tcp", "link", 87,
	"tcp", "supdup", 95,
	"tcp", "hostnames", 101,
	"tcp", "iso-tsap", 102,
	"tcp", "x400", 103,
	"tcp", "x400-snd", 104,
	"tcp", "csnet-ns", 105,
	"tcp", "pop-2", 109,
	"tcp", "pop3", 110,
	"tcp", "portmap", 111,
	"tcp", "uucp-path", 117,
	"tcp", "nntp", 119,
	"tcp", "netbios", 139,
	"tcp", "imap4", 143,
	"tcp", "imap", 143,
	"tcp", "NeWS", 144,
	"tcp", "print-srv", 170,
	"tcp", "z39.50", 210,
	"tcp", "fsb", 400,
	"tcp", "sysmon", 401,
	"tcp", "proxy", 402,
	"tcp", "proxyd", 404,
	"tcp", "https", 443,
	"tcp", "cifs", 445,
	"tcp", "ssmtp", 465,
	"tcp", "rexec", 512,
	"tcp", "login", 513,
	"tcp", "shell", 514,
	"tcp", "printer", 515,
	"tcp", "ncp", 524,
	"tcp", "courier", 530,
	"tcp", "cscan", 531,
	"tcp", "uucp", 540,
	"tcp", "snntp", 563,
	"tcp", "9fs", 564,
	"tcp", "whoami", 565,
	"tcp", "guard", 566,
	"tcp", "ticket", 567,
	"tcp", "fmclient", 729,
	"tcp", "imaps", 993,
	"tcp", "pop3s", 995,
	"tcp", "ingreslock", 1524,
	"tcp", "pptp", 1723,
	"tcp", "nfs", 2049,
	"tcp", "webster", 2627,
	"tcp", "weather", 3000,
	"tcp", "sip", 5060,
	"tcp", "sips", 5061,
	"tcp", "secstore", 5356,
	"tcp", "vnc-http", 5800,
	"tcp", "vnc", 5900,
	"tcp", "Xdisplay", 6000,
	"tcp", "styx", 6666,
	"tcp", "mpeg", 6667,
	"tcp", "rstyx", 6668,
	"tcp", "infdb", 6669,
	"tcp", "infsigner", 6671,
	"tcp", "infcsigner", 6672,
	"tcp", "inflogin", 6673,
	"tcp", "bandt", 7330,
	"tcp", "face", 32000,
	"tcp", "dhashgate", 11978,
	"tcp", "exportfs", 17007,
	"tcp", "rexexec", 17009,
	"tcp", "ncpu", 17010,
	"tcp", "cpu", 17013,
	"tcp", "glenglenda1", 17020,
	"tcp", "glenglenda2", 17021,
	"tcp", "glenglenda3", 17022,
	"tcp", "glenglenda4", 17023,
	"tcp", "glenglenda5", 17024,
	"tcp", "glenglenda6", 17025,
	"tcp", "glenglenda7", 17026,
	"tcp", "glenglenda8", 17027,
	"tcp", "glenglenda9", 17028,
	"tcp", "glenglenda10", 17029,
	"tcp", "flyboy", 17032,
	"tcp", "venti", 17034,
	"tcp", "wiki", 17035,
	"tcp", "vica", 17036,

//	"il", "9fs", 17008,

	"udp", "echo", 7,
	"udp", "tacacs", 49,
	"udp", "tftp", 69,
	"udp", "bootpc", 68,
	"udp", "bootp", 67,
	"udp", "domain", 53,
	"udp", "dns", 53,
	"udp", "portmap", 111,
	"udp", "ntp", 123,
	"udp", "netbios-ns", 137,
	"udp", "snmp", 161,
	"udp", "syslog", 514,
	"udp", "rip", 520,
	"udp", "dhcp6c", 546,
	"udp", "dhcp6s", 547,
	"udp", "nfs", 2049,
	"udp", "bfs", 2201,
	"udp", "virgil", 2202,
	"udp", "sip", 5060,
	"udp", "bandt2", 7331,
	"udp", "oradius", 1645,
	"udp", "dhash", 11977,
	0
};

static int
lookupport(char *s, char **pproto)
{
	int i;
	char buf[10], *p, *proto;

	i = strtol(s, &p, 0);
	if(*s && *p == 0)
		return i;

	proto = *pproto;
	if(strcmp(proto, "net") == 0)
		proto = nil;
	if(proto == nil){
		if(so_getservbyname(s, "tcp", buf) >= 0){
			*pproto = "tcp";
			return atoi(buf);
		}
		if(so_getservbyname(s, "udp", buf) >= 0){
			*pproto = "udp";
			return atoi(buf);
		}
	}else{
		if(strcmp(proto, "tcp") != 0 && strcmp(proto, "udp") != 0)
			return 0;
		if(so_getservbyname(s, proto, buf) >= 0){
			*pproto = "tcp";
			return atoi(buf);
		}
	}
	for(i=0; tab[i].proto; i++){
		if(proto == nil || strcmp(proto, tab[i].proto) == 0)
		if(strcmp(s, tab[i].name) == 0){
			if(proto == nil)
				*pproto = tab[i].proto;
			return tab[i].num;
		}
	}
	return 0;
}

static int
lookuphost(char *s, uchar *to)
{
	ulong ip;
	char *p;

	memset(to, 0, 4);
	p = v4parseip(to, s);
	if(p && *p == 0 && (ip = nhgetl(to)) != 0)
		return 0;
	if((s = hostlookup(s)) == nil)
		return -1;
	v4parseip(to, s);
	free(s);
	return 0;
}

long
cswrite(Chan *c, void *a, long n, vlong offset)
{
	char *f[4];
	char *s, *ns;
	int nf, port, bang;
	uchar ip[4];

	s = malloc(n+1);
	if(s == nil)
		error(Enomem);
	ns = malloc(128);
	if(ns == nil){
		free(s);
		error(Enomem);
	}
	if(waserror()){
		free(s);
		free(ns);
		nexterror();
	}
	memmove(s, a, n);
	s[n] = 0;
	
	if(TYPE(c->qid) == Qcs){
		nf = getfields(s, f, nelem(f), 0, "!");
		if(nf != 3)
			error("bad syntax");

		port = lookupport(f[2], &f[0]);
		if(port <= 0)
			error("no translation for port found");

		if(lookuphost(f[1], ip) < 0)
			error("no translation for host found");
		snprint(ns, 128, "/net/%s/clone %V!%d", f[0], ip, port);
	}else{
		/* dns */
		bang = 0;
		if(s[0] == '!')
			bang = 1;
		nf = tokenize(s+bang, f, nelem(f));
		if(nf > 2)
			error("bad syntax");
		if(nf > 1 && strcmp(f[1], "ip") != 0)
			error("can only lookup ip addresses");
		if(lookuphost(f[0], ip) < 0)
			error("no translation for host found");
		if(bang)
			snprint(ns, 128, "dom=%s ip=%V", f[0], ip);
		else
			snprint(ns, 128, "%s ip\t%V", f[0], ip);
	}
	free(c->aux);
	c->aux = ns;
	poperror();
	free(s);
	return n;
}

Dev ipdevtab = 
{
	'I',
	"ip",

	devreset,
	ipinit,
	devshutdown,
	ipattach,
	ipwalk,
	ipstat,
	ipopen,
	devcreate,
	ipclose,
	ipread,
	devbread,
	ipwrite,
	devbwrite,
	ipremove,
	devwstat,
};

