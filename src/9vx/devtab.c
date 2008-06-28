#include "u.h"
#include "mem.h"
#include "lib.h"
#include "dat.h"
#include "fns.h"
#include "error.h"

extern Dev consdevtab;
extern Dev rootdevtab;
extern Dev pipedevtab;
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

Dev *devtab[] = {
	&rootdevtab,	/* must be first */
	&audiodevtab,
	&consdevtab,
	&drawdevtab,
	&dupdevtab,
	&envdevtab,
	&fsdevtab,
	&ipdevtab,
	&mntdevtab,
	&mntloopdevtab,
	&mousedevtab,
	&pipedevtab,
	&procdevtab,
	&srvdevtab,
	&ssldevtab,
	&tlsdevtab,
	&sddevtab,
	0
};

