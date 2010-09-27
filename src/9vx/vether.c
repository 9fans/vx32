#include "u.h"
#include "mem.h"
#include "lib.h"
#include "dat.h"
#include "fns.h"
#include "error.h"
#include "ip/ip.h"
#include "netif.h"
#include "etherif.h"
#include "vether.h"
#include "sd.h"

extern int nettap;
extern void ethertaplink(void);
extern void etherpcaplink(void);
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
	for(int i=0; i<MaxEther; i++){
		if(ve[i].dev == nil)
			continue;
		if(ve[i].tap == 1)
			ethertaplink();
		else
			etherpcaplink();
	}
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

SDifc *sdifc[] =
{
	&sdloopifc,
	&sdaoeifc,
	0,
};
