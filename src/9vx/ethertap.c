/*
 * ethertap: tap device ethernet driver
 * copyright © 2008 erik quanstrom
 * copyright © 2010 Tully Gray
 * copyright © 2010 Jesus Galan Lopez
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

#include <net/if.h>
#include <sys/ioctl.h>

#ifdef linux
#include <netpacket/packet.h>
#include <linux/if_tun.h>
#elif defined(__FreeBSD__)
#include <net/if_tun.h>
#endif

typedef struct Ctlr Ctlr;
struct Ctlr {
	int	fd;
	int	txerrs;
	uchar	ea[Eaddrlen];
};

static	uchar	anyea[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,};

#ifdef linux
static int
opentap(char *dev)
{
	int fd;
	char *tap0 = "tap0";
	struct ifreq ifr;

	if(dev == nil)
		dev = tap0;
	if((fd = open("/dev/net/tun", O_RDWR)) < 0)
		return -1;
	memset(&ifr, 0, sizeof ifr);
	strncpy(ifr.ifr_name, dev, sizeof ifr.ifr_name);
	ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
	if(ioctl(fd, TUNSETIFF, &ifr) < 0){
		close(fd);
		return -1;
	}
	return fd;
}
#elif defined(__FreeBSD__)
static int
opentap(char *dev)
{
	int fd;
	struct stat s;

	if((fd = open("/dev/tap", O_RDWR)) < 0)
		return -1;
	return fd;
}
#endif

static int
setup(char *dev)
{
	return opentap(dev);
}

Block*
tappkt(Ctlr *c)
{
	int n;
	Block *b;

	b = allocb(1514);
	for(;;){
		n = read(c->fd, b->rp, BALLOC(b));
		if(n <= 0)
			panic("fd %d read %d", c->fd, n);
		if(memcmp(b->rp + 0, anyea, 6) == 0
		|| memcmp(b->rp + 0, c->ea, 6) == 0)
			break;
	}
	b->wp += n;
	b->flag |= Btcpck|Budpck|Bpktck;
	return b;
}

static void
taprecvkproc(void *v)
{
	Block *b;
	Ether *e;

	e = v;
	while((b = tappkt(e->ctlr)))
		etheriq(e, b, 1);
	pexit("read fail", 1);
}

static void
taptransmit(Ether* e)
{
	Block *b, *h;
	Ctlr *c;

	c = e->ctlr;
	while ((b = qget(e->oq)) != nil) {
		if(memcmp(b->rp + 6, anyea, 6) == 0 ||
		memcmp(b->rp + 0, c->ea, 6) == 0){
			h = allocb(BLEN(b));
			memcpy(h->rp, b->wp, BLEN(b));
			h->wp += BLEN(b);
			h->flag |= Btcpck|Budpck|Bpktck;
			etheriq(e, h, 1);
		}
		if(write(c->fd, b->rp, BLEN(b)) == -1)
			c->txerrs++;
		freeb(b);
	}
}

static long
tapifstat(Ether *e, void *a, long n, ulong offset)
{
	char buf[128];
	Ctlr *c;

	c = a;
	snprint(buf, sizeof buf, "txerrors: %lud\n", c->txerrs);
	return readstr(offset, a, n, buf);
}

static void
tapattach(Ether* e)
{
	kproc("taprecv", taprecvkproc, e);
}

static int
tappnp(Ether* e)
{
	Ctlr c;
	static int cve = 0;

	while(cve < nve && ve[cve].tap == 0)
		cve++;
	if(cve == nve)
		return -1;

	memset(&c, 0, sizeof c);
	c.fd = setup(ve[cve].dev);
	memcpy(c.ea, ve[cve].ea, Eaddrlen);
	if(c.fd== -1){
		iprint("ve: tap failed to initialize\n");
		cve++;
		return -1;
	}
	e->ctlr = malloc(sizeof c);
	memcpy(e->ctlr, &c, sizeof c);
	e->tbdf = BUSUNKNOWN;
	memcpy(e->ea, ve[cve].ea, Eaddrlen);
	e->attach = tapattach;
	e->transmit = taptransmit;
	e->ifstat = tapifstat;
	e->ni.arg = e;
	e->ni.link = 1;
	cve++;
	return 0;
}

void
ethertaplink(void)
{
	addethercard("tap", tappnp);
}
