#include "u.h"
#include "lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "error.h"

#include "ip.h"

static void
nullbind(Ipifc* _, int __, char** ___)
{
	error("cannot bind null device");
}

static void
nullunbind(Ipifc* _)
{
}

static void
nullbwrite(Ipifc* _, Block* __, int ___, uchar* ____)
{
	error("nullbwrite");
}

Medium nullmedium =
{
.name=		"null",
.bind=		nullbind,
.unbind=	nullunbind,
.bwrite=	nullbwrite,
};

void
nullmediumlink(void)
{
	addipmedium(&nullmedium);
}
