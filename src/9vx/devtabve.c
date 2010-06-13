#include "u.h"
#include "mem.h"
#include "lib.h"
#include "dat.h"
#include "fns.h"
#include "error.h"
#include "ip/ip.h"
#include "sd.h"

extern Dev aoedevtab;
extern Dev consdevtab;
extern Dev rootdevtab;
extern Dev pipedevtab;
extern Dev ramdevtab;
extern Dev ssldevtab;
extern Dev tlsdevtab;
extern Dev mousedevtab;
extern Dev drawdevtab;
extern Dev ipdevtab;
extern Dev fsdevtab;
extern Dev mntdevtab;
extern Dev audiodevtab;
extern Dev envdevtab;
extern Dev srvdevtab;
extern Dev procdevtab;
extern Dev mntloopdevtab;
extern Dev dupdevtab;
extern Dev sddevtab;
extern Dev capdevtab;
extern Dev etherdevtab;

extern SDifc sdloopifc;
extern SDifc sdaoeifc;

Dev *devtab[] = {
	&rootdevtab,	/* must be first */
	&aoedevtab,
	&audiodevtab,
	&capdevtab,
	&consdevtab,
	&drawdevtab,
	&dupdevtab,
	&envdevtab,
	&etherdevtab,
	&fsdevtab,
	&ipdevtab,
	&mntdevtab,
	&mntloopdevtab,
	&mousedevtab,
	&pipedevtab,
	&procdevtab,
	&ramdevtab,
	&sddevtab,
	&srvdevtab,
	&ssldevtab,
	&tlsdevtab,
	0
};

extern int nettap;
extern void ethertaplink(void);
extern void ethervelink(void);
extern void ethermediumlink(void);
extern void loopbackmediumlink(void);
extern void netdevmediumlink(void);
void links(void) {
	ethermediumlink();
	loopbackmediumlink();
	netdevmediumlink();
	if(nettap)
		ethertaplink();
	else
		ethervelink();
}

extern void ilinit(Fs*);
extern void tcpinit(Fs*);
extern void udpinit(Fs*);
extern void ipifcinit(Fs*);
extern void icmpinit(Fs*);
extern void icmp6init(Fs*);
extern void greinit(Fs*);
extern void ipmuxinit(Fs*);
extern void espinit(Fs*);
void (*ipprotoinit[])(Fs*) = {
	ilinit,
	tcpinit,
	udpinit,
	ipifcinit,
	icmpinit,
	icmp6init,
	greinit,
	ipmuxinit,
	espinit,
	nil,
};

int
eafrom(char *ma, uchar ea[6])
{
	int i;
	char **nc = &ma;

	for(i = 0; i < 6; i++){
		if(!ma)
			return -1;
		ea[i] = (uchar)strtoul(ma, nc, 16);
		ma = *nc+1;
	}
	return 0;
}

SDifc *sdifc[] =
{
	&sdloopifc,
	&sdaoeifc,
	0,
};
