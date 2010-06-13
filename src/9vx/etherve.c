/*
 * etherve - portable Virtual Ethernet driver for 9vx.
 * 
 * Copyright (c) 2008 Devon H. O'Dell
 * copyright © 2008 erik quanstrom
 * copyright © 2010 Jesus Galan Lopez
 *
 * Released under 2-clause BSD license.
 */

#include "u.h"

#include "a/lib.h"
#include "a/mem.h"
#include "a/dat.h"
#include "a/fns.h"
#include "a/io.h"
#include "a/error.h"
#include "a/netif.h"

#include "a/etherif.h"

#include <pcap.h>

extern	char	*macaddr;
extern	char	*netdev;
static	uvlong	txerrs;

extern	int	eafrom(char *ma, uchar ea[6]);

typedef struct Ctlr Ctlr;
struct Ctlr {
	pcap_t	*pd;
};

static uchar ea[6] = {0x00, 0x48, 0x01, 0x23, 0x45, 0x67};

static void *
veerror(char* err)
{
	iprint("ve: %s\n", err);
	return nil;
}

static pcap_t *
setup(void)
{
	char	filter[30] = "ether dst 00:48:01:23:45:67";
	char	errbuf[PCAP_ERRBUF_SIZE];
	pcap_t	*pd;
	struct bpf_program prog;
	bpf_u_int32 net;
	bpf_u_int32 mask;

	if(macaddr){
		if(strlen(macaddr)>17)
			return veerror("wrong mac address");
		else if(sprintf(filter, "ether dst %s", macaddr) == -1)
			return veerror("cannot create pcap filter");
	}

	if (!netdev && (netdev = pcap_lookupdev(errbuf)) == nil)
		return veerror("cannot find network device");

//	if ((pd = pcap_open_live(netdev, 1514, 1, 1, errbuf)) == nil)
	if ((pd = pcap_open_live(netdev, 65000, 1, 1, errbuf)) == nil)
		return nil;

	if (macaddr && (eafrom(macaddr, ea) == -1))
		return veerror("cannot read mac address");

	pcap_lookupnet(netdev, &net, &mask, errbuf);
	pcap_compile(pd, &prog, filter, 0, net);

	if (pcap_setfilter(pd, &prog) == -1)
		return nil;

	pcap_freecode(&prog);

	return pd;
}

static Block *
vepkt(Ctlr *c)
{
	struct pcap_pkthdr hdr;
	uchar *p, *q;
	Block *b;

	while ((p = pcap_next(c->pd, &hdr)) == nil);

	b = allocb(hdr.caplen);
	b->wp += hdr.caplen;
	for(q = b->rp; q != b->wp; q++)
		*q = *(p++);
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
verecvkproc(void *v)
{
	Ether *e;
	Block *b;

	e = v;
	while ((b = vepkt(e->ctlr))) 
		if (b != nil)
			etheriq(e, b, 1);
}

static void
vetransmit(Ether* e)
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
veifstat(Ether *e, void *a, long n, ulong offset)
{
	char buf[128];

	snprint(buf, sizeof buf, "txerrors: %lud\n", txerrs);
	return readstr(offset, a, n, buf);
}

static void
veattach(Ether* e)
{
	kproc("verecv", verecvkproc, e);
}

static int
vepnp(Ether* e)
{
	Ctlr c;
	static int nctlr = 0;

	if (nctlr++ > 0)
		return -1;

	memset(&c, 0, sizeof(c));
	c.pd = setup();
	if (c.pd == nil) {
		iprint("ve: pcap failed to initialize\n");
		return -1;
	}
	e->ctlr = malloc(sizeof(c));
	memcpy(e->ctlr, &c, sizeof(c));
	e->tbdf = BUSUNKNOWN;
	memcpy(e->ea, ea, sizeof(ea));
	e->attach = veattach;
	e->transmit = vetransmit;
	e->ifstat = veifstat;
	e->ni.arg = e;
	e->ni.link = 1;
	return 0;
}

void
ethervelink(void)
{
	addethercard("ve", vepnp);
}
