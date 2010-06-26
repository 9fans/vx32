/*
 * plimit-bsd.c - Process limiting support for BSD systems.
 *
 * Copyright (c) 2008 by Devon H. O'Dell <devon.odell@gmail.com>
 *
 * This software is released under a 2-clause BSD license.
 */

#include <sys/types.h>
#include <sys/proc.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/timespec.h>
#include <sys/user.h>

#include <fcntl.h>
#include <kvm.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "u.h"

#define timediff(x, y)						\
	(((x)->tv_sec - (y)->tv_sec) * 1000000 +		\
	((x)->tv_usec - (y)->tv_usec))

void
plimit(pid_t pid, int percent)
{
	pid_t p;
	double lim;

	lim = (double)percent; // XXX: / 100 ?

	p = rfork(RFPROC|RFNOWAIT|RFCFDG);

	if (p == 0) {
		struct timespec sleep_slice;
		struct timespec work_slice;
		struct timeval last_sample;
		struct timeval last_start;
		struct timeval last_end;
		struct clockinfo ci;
		double last_usage;
		int last_pstart;
		int mibctl[2];
		size_t len;
		double rat;
		double cpu;
		int c;

		mibctl[0] = CTL_KERN;
		mibctl[1] = KERN_CLOCKRATE;
		len = sizeof(ci);
		sysctl(mibctl, 2, &ci, &len, NULL, 0);

		rat = cpu = -1;
		last_usage = 0.0;
		c = last_pstart = -1;

		memset(&sleep_slice, 0, sizeof(struct timespec));
		memset(&work_slice, 0, sizeof(struct timespec));
		memset(&last_sample, 0, sizeof(struct timeval));
		memset(&last_start, 0, sizeof(struct timeval));
		memset(&last_end, 0, sizeof(struct timeval));

		while (1) {
			struct kinfo_proc *kp;
			struct timeval now;
			struct proc kp_p;
			kvm_t *kd;
			long dt;
			uint64_t mt;
			int t;

			kd = kvm_open(NULL, NULL, NULL, O_RDONLY, NULL);
			if (!kd) {
				return;
			}

			kp = kvm_getprocs(kd, KERN_PROC_PID, pid, &t);
			if (!kp) {
				return;
			}

			if (kvm_read(kd, (ulong)kp->ki_paddr, &kp_p, sizeof (struct proc)) == -1) {
				return;
			}

			mt = kp_p.p_rux.rux_tu + kp_p.p_crux.rux_tu;

			kvm_close(kd);

			gettimeofday(&now, NULL);
			if (last_pstart < 0) {
				last_sample = now;
				last_pstart = mt;

				kill(pid, SIGSTOP);
				continue;
			}

			dt = timediff(&now, &last_sample);

			if (last_usage == 0.0) {
				last_usage = ((mt - last_pstart) / (dt * ci.hz / 1000000.0));
			} else {
				last_usage = 0.96 * last_usage + 0.96 * ((mt - last_pstart) / (dt * ci.hz / 1000000.0));
			}

			last_sample = now;
			last_pstart = mt;

			if (cpu < 0) {
				cpu = lim;
				rat = lim;
				work_slice.tv_nsec = 100000000 * lim;
			} else {
				rat = MIN(rat / cpu * lim, 1);
				work_slice.tv_nsec = 100000000 * rat;
			}

			sleep_slice.tv_nsec = 100000000 - work_slice.tv_nsec;

			kill(pid, SIGCONT);

			gettimeofday(&last_start, NULL);
			nanosleep(&work_slice, NULL);
			gettimeofday(&last_end, NULL);

			if (sleep_slice.tv_nsec > 0) {
				kill(pid, SIGSTOP);
				nanosleep(&sleep_slice, NULL);
			}
		}
	} else {
		return;
	}

	return;
}
