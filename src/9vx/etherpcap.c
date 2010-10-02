/*
 * etherpcap - portable Virtual Ethernet driver for 9vx.
 * 
 * Copyright (c) 2008 Devon H. O'Dell
 * copyright © 2008 erik quanstrom
 * copyright © 2010 Jesus Galan Lopez
 *
 * Released under 2-clause BSD license.
 */

#include "u.h"

#include "lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "io.h"
#include "error.h"
#include "netif.h"
#include "etherif.h"
#include "vether.h"

#include <pcap.h>

static	uvlong	txerrs;

extern	int	eafrom(char *ma, uchar ea[6]);

typedef struct Ctlr Ctlr;
struct Ctlr {
	pcap_t	*pd;
};

static void *
veerror(char* err)
{
	iprint("ve: %s\n", err);
	return nil;
}

static pcap_t *
setup(char *dev, uchar *ea)
{
	char	filter[30];
	char	errbuf[PCAP_ERRBUF_SIZE];
	pcap_t	*pd;
	struct bpf_program prog;
	bpf_u_int32 net;
	bpf_u_int32 mask;

	if(sprint(filter, "ether dst %2.2ux:%2.2ux:%2.2ux:%2.2ux:%2.2ux:%2.2ux",
	ea[0], ea[1], ea[2],ea[3], ea[4], ea[5]) == -1)
		return veerror("cannot create pcap filter");

	if ((pd = pcap_open_live(dev, 65000, 1, 1, errbuf)) == nil){
		// try to find a device
		if ((dev = pcap_lookupdev(errbuf)) == nil)
			return veerror("cannot find network device");
		if ((pd = pcap_open_live(dev, 65000, 1, 1, errbuf)) == nil)
			return nil;
	}

	pcap_lookupnet(dev, &net, &mask, errbuf);
	pcap_compile(pd, &prog, filter, 0, net);

	if (pcap_setfilter(pd, &prog) == -1)
		return nil;

	pcap_freecode(&prog);

	return pd;
}

static Block *
pcappkt(Ctlr *c)
{
	struct pcap_pkthdr hdr;
	uchar *p;
	Block *b;

	while ((p = pcap_next(c->pd, &hdr)) == nil);

	b = allocb(hdr.caplen);
	memcpy(b->rp, p, hdr.caplen);
	b->wp += hdr.caplen;
	b->flag |= Btcpck|Budpck|Bpktck;

/*
	iprint("+++++++++++ packet %d (len %d):\n", ++fn, hdr.caplen);
	int i=0; uchar* u;
	static int fn=0;

	for(u=b->rp; u<b->wp; u++){
		if (i%16 == 0) iprint("%.4ux", i);
		if (i%8 == 0) iprint("   ");
		iprint("%2.2ux ", *u);
		if (++i%16 == 0) iprint("\n");
	}
	iprint("\n-------------\n");
*/

	return b;

}

static void
pcaprecvkproc(void *v)
{
	Ether *e;
	Block *b;

	e = v;
	while ((b = pcappkt(e->ctlr))) 
		if (b != nil)
			etheriq(e, b, 1);
}

static void
pcaptransmit(Ether* e)
{
	const u_char *u;
	Block *b;
	Ctlr *c;

	c = e->ctlr;
	while ((b = qget(e->oq)) != nil) {
		int wlen;

		u = (const u_char*)b->rp;

		wlen = pcap_inject(c->pd, u, BLEN(b));
		// iprint("injected packet len %d\n", wlen);
		if (wlen == -1)
			txerrs++;

		freeb(b);
	}
}

static long
pcapifstat(Ether *e, void *a, long n, ulong offset)
{
	char buf[128];

	snprint(buf, sizeof buf, "txerrors: %lud\n", txerrs);
	return readstr(offset, a, n, buf);
}

static void
pcapattach(Ether* e)
{
	kproc("pcaprecv", pcaprecvkproc, e);
}

static int
pcappnp(Ether* e)
{
	Ctlr c;
	static int cve = 0;

	while(cve < MaxEther && ve[cve].tap == 1)
		cve++;
	if(cve == MaxEther || ve[cve].dev == nil)
		return -1;

	memset(&c, 0, sizeof(c));
	c.pd = setup(ve[cve].dev, ve[cve].ea);
	if (c.pd == nil) {
		iprint("ve: pcap failed to initialize\n");
		cve++;
		return -1;
	}
	e->ctlr = malloc(sizeof(c));
	memcpy(e->ctlr, &c, sizeof(c));
	e->tbdf = BUSUNKNOWN;
	memcpy(e->ea, ve[cve].ea, Eaddrlen);
	e->attach = pcapattach;
	e->transmit = pcaptransmit;
	e->ifstat = pcapifstat;
	e->ni.arg = e;
	e->ni.link = 1;
	cve++;
	return 0;
}

void
etherpcaplink(void)
{
	addethercard("pcap", pcappnp);
}
