#include "u.h"
#include "lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "error.h"

#include "ip.h"


static void	pktbind(Ipifc*, int, char**);
static void	pktunbind(Ipifc*);
static void	pktbwrite(Ipifc*, Block*, int, uchar*);
static void	pktin(Fs*, Ipifc*, Block*);

Medium pktmedium =
{
.name=		"pkt",
.hsize=		14,
.mintu=		40,
.maxtu=		4*1024,
.maclen=	6,
.bind=		pktbind,
.unbind=	pktunbind,
.bwrite=	pktbwrite,
.pktin=		pktin,
};

/*
 *  called to bind an IP ifc to an ethernet device
 *  called with ifc wlock'd
 */
static void
pktbind(Ipifc* _, int argc, char **argv)
{
}

/*
 *  called with ifc wlock'd
 */
static void
pktunbind(Ipifc* _)
{
}

/*
 *  called by ipoput with a single packet to write
 */
static void
pktbwrite(Ipifc *ifc, Block *bp, int _, uchar* __)
{
	/* enqueue onto the conversation's rq */
	bp = concatblock(bp);
	if(ifc->conv->snoopers.ref > 0)
		qpass(ifc->conv->sq, copyblock(bp, BLEN(bp)));
	qpass(ifc->conv->rq, bp);
}

/*
 *  called with ifc rlocked when someone write's to 'data'
 */
static void
pktin(Fs *f, Ipifc *ifc, Block *bp)
{
	if(ifc->lifc == nil)
		freeb(bp);
	else {
		if(ifc->conv->snoopers.ref > 0)
			qpass(ifc->conv->sq, copyblock(bp, BLEN(bp)));
		ipiput4(f, ifc, bp);
	}
}

void
pktmediumlink(void)
{
	addipmedium(&pktmedium);
}
