/*
 * ethertap: tap device ethernet driver
 * copyright © 2008 erik quanstrom
 * copyright © 2010 Tully Gray
 * copyright © 2010 Jesus Galan Lopez
 */

#include "u.h"
#include <sys/socket.h>
#include <net/if.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <sys/ioctl.h>

#include "a/lib.h"
#include "a/mem.h"
#include "a/dat.h"
#include "a/fns.h"
#include "a/io.h"
#include "a/error.h"
#include "a/netif.h"

#include "a/etherif.h"

#ifdef linux
#include <netpacket/packet.h>
#include <linux/if_tun.h>
#elif defined(__FreeBSD__)
#include <net/if_tun.h>
#endif

extern	char	*macaddr;
extern	char	*netdev;

extern	int	eafrom(char *ma, uchar ea[6]);

typedef struct Ctlr Ctlr;
struct Ctlr {
	int	fd;
	int	txerrs;
	int	promisc;
	uchar	ea[Eaddrlen];
};

static	uchar	anyea[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,};
static	uchar	ea[6] = {0x00, 0x48, 0x01, 0x23, 0x45, 0x67};

#ifdef linux
static int
opentap(void)
{
	int fd;
	char *dev = "tap0";
	struct ifreq ifr;

	if(netdev)
		dev = netdev;
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
opentap(void)
{
	int fd;
	struct stat s;

	if((fd = open("/dev/tap", O_RDWR)) < 0)
		return -1;
	return fd;
}
#endif

static int
setup(void)
{
	if (macaddr && (eafrom(macaddr, ea) == -1)){
		iprint("ve: cannot read mac address\n");
		return -1;
	}
	return opentap();
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
		if(c->promisc == 0)
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
	static int nctlr;

	if(nctlr++ > 0)
		return -1;
	memset(&c, 0, sizeof c);
	c.fd = setup();
	memcpy(c.ea, ea, Eaddrlen);
	if(c.fd== -1){
		iprint("ve: tap failed to initialize\n");
		return -1;
	}
	e->ctlr = malloc(sizeof c);
	memcpy(e->ctlr, &c, sizeof c);
	e->tbdf = BUSUNKNOWN;
	memcpy(e->ea, ea, Eaddrlen);
	e->attach = tapattach;
	e->transmit = taptransmit;
	e->ifstat = tapifstat;
	e->ni.arg = e;
	e->ni.link = 1;
	return 0;
}

void
ethertaplink(void)
{
	addethercard("tap", tappnp);
}
