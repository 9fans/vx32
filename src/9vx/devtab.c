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
	&srvdevtab,
	&ssldevtab,
	&tlsdevtab,
	&sddevtab,
	&capdevtab,
	0
};

extern void ethervelink(void);
extern void ethermediumlink(void);
extern void loopbackmediumlink(void);
extern void netdevmediumlink(void);
void links(void) {
	ethermediumlink();
	loopbackmediumlink();
	netdevmediumlink();
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

SDifc *sdifc[] =
{
	&sdloopifc,
	&sdaoeifc,
	0,
};
