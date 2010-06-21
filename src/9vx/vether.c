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

void
setmac(char *macaddr)
{
	int i;
	char **nc = &macaddr;

	if(nve == 0)
		return;
	ve[nve-1].mac = macaddr;
	for(i = 0; i < Eaddrlen; i++){
		ve[nve-1].ea[i] = (uchar)strtoul(macaddr, nc, 16);
		macaddr = *nc+1;
	}
}

static int
eainuse(int n, uchar ea[Eaddrlen])
{
	int i;

	for(i = 0; i < nve; i++)
		if((i<n || ve[i].mac != nil) && memcmp(ea, ve[i].ea, Eaddrlen) == 0)
			return -1;
	return 0;
}

void
addve(char *dev, int tap)
{
	if(nve == MaxEther)
		panic("too many virtual ether cards");
	ve[nve].tap = tap;
	ve[nve].dev = dev;
	ve[nve].mac = nil;
	nve++;
}

void links(void) {
	static uchar ea[Eaddrlen] = {0x00, 0x00, 0x09, 0x00, 0x00, 0x00};

	ethermediumlink();
	loopbackmediumlink();
	netdevmediumlink();
	for(int i=0; i<nve; i++){
		if(ve[i].mac == nil){
			while(eainuse(i, ea))
				ea[5]++;
			memcpy(ve[i].ea, ea, Eaddrlen);
		}
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
