#include "u.h"
#include "mem.h"
#include "lib.h"
#include "dat.h"
#include "fns.h"
#include "error.h"

extern Dev aoedevtab;
extern Dev consdevtab;
extern Dev rootdevtab;
extern Dev pipedevtab;
extern Dev ramdevtab;
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
extern Dev capdevtab;
extern Dev etherdevtab;

Dev *devtab[] = {
	&rootdevtab,	/* must be first */
	&aoedevtab,
	&audiodevtab,
	&capdevtab,
	&consdevtab,
	&drawdevtab,
	&dupdevtab,
	&envdevtab,
	&etherdevtab,
	&fsdevtab,
	&ipdevtab,
	&mntdevtab,
	&mntloopdevtab,
	&mousedevtab,
	&pipedevtab,
	&procdevtab,
	&ramdevtab,
	&sddevtab,
	&srvdevtab,
	&ssldevtab,
	&tlsdevtab,
	0
};
