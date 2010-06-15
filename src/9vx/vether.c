#include "u.h"
#include "mem.h"
#include "lib.h"
#include "dat.h"
#include "fns.h"
#include "error.h"
#include "ip/ip.h"
#include "sd.h"

extern int nettap;
extern void ethertaplink(void);
extern void ethervelink(void);
extern void ethermediumlink(void);
extern void loopbackmediumlink(void);
extern void netdevmediumlink(void);

extern void ilinit(Fs*);
extern void tcpinit(Fs*);
extern void udpinit(Fs*);
extern void ipifcinit(Fs*);
extern void icmpinit(Fs*);
extern void icmp6init(Fs*);
extern void greinit(Fs*);
extern void ipmuxinit(Fs*);
extern void espinit(Fs*);

extern SDifc sdloopifc;
extern SDifc sdaoeifc;

void links(void) {
	ethermediumlink();
	loopbackmediumlink();
	netdevmediumlink();
	if(nettap)
		ethertaplink();
	else
		ethervelink();
}

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
