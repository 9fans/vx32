#include	"u.h"
#include	<sys/times.h>
#include	"lib.h"
#include	"mem.h"
#include	"dat.h"

/*
 * CPU load
 */
void
ploadproc(void *v)
{
	double load;
	vlong hz;
	struct tms t;
	clock_t now, last;
	clock_t used, lastused;

	hz = sysconf(_SC_CLK_TCK);
	last = 0;
	lastused = 0;
	load = 0;

	for(;;){
		usleep(1000000);
		now = times(&t);
		used = t.tms_utime + t.tms_stime;
		load = (double)(used - lastused) / (now - last);
		machp[0]->load = load * 100 * hz;
/*
iprint("XXX Load: %d%%\n", (int)(load * 100));
*/
		lastused = used;
		last = now;
	}
}

