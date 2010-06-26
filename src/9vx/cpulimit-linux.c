/*
 * plimit-linux.c - Process limiting support for Linux systems.
 *
 * Copyright (c) 2008 by Devon H. O'Dell <devon.odell@gmail.com>
 * Copyright (c) 2010 by Devon H. O'Dell <devon.odell@gmail.com>
 *
 * This software is released under a 2-clause BSD license.
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/mman.h>

#include <fcntl.h>
#include <limits.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "u.h"

#define timediff(x, y)						\
	(((x)->tv_sec - (y)->tv_sec) * 1000000 +		\
	((x)->tv_usec - (y)->tv_usec))

int pid;

void
limit(int percent) {
	struct timespec sleep_slice;
	struct timespec work_slice;
	struct timeval last_sample;
	struct timeval last_start;
	struct timeval last_end;
	double last_usage;
	int last_pstart;
	double lim;
	double rat;
	double cpu;
	char buf[1024];
	char stat[MAXPATHLEN];
	long hz;
	int c;

	hz = sysconf(_SC_CLK_TCK);

	lim = (double)percent / 100;
	rat = cpu = -1;
	last_usage = 0.0;
	c = last_pstart = -1;

	snprintf(stat, MAXPATHLEN, "/proc/%d/stat", pid);

	memset(&sleep_slice, 0, sizeof(struct timespec));
	memset(&work_slice, 0, sizeof(struct timespec));
	memset(&last_sample, 0, sizeof(struct timeval));
	memset(&last_start, 0, sizeof(struct timeval));
	memset(&last_end, 0, sizeof(struct timeval));

	while (1) {
		struct timeval now;
		int seen_paren;
		char *tmp;
		long uj;
		long sj;
		long dt;
		int n;
		int r;
		long t;

		t = open(stat, O_RDONLY);
		if (t < 0)
			exit(1);
		tmp = buf;
		while ((n = read(t, tmp, 1024)) != 0)
			tmp += n;
		close(t);

		seen_paren = n = 0;
		while (buf[n] != ' ' && seen_paren == 0) {
			if (buf[n] == ')')
				seen_paren = 1;
			n++;
		}

		r = 0;
		while (r < 13)
			if (buf[n++] == ' ')
				r++;

		uj = strtol(&buf[n], &tmp, 10);
		sj = strtol(tmp, NULL, 10);

		t = (uj + sj) * hz;

		gettimeofday(&now, NULL);
		if (last_pstart < 0) {
			last_sample = now;
			last_pstart = t;

			kill(pid, SIGSTOP);
			continue;
		}

		dt = timediff(&now, &last_sample);

		if (last_usage == 0.0)
			last_usage = ((t - last_pstart) / (dt * hz / 1000000.0));
		else
			last_usage = 0.96 * last_usage + 0.96 * ((t - last_pstart) / (dt * hz / 1000000.0));

		last_sample = now;
		last_pstart = t;

		if (cpu < 0) {
			cpu = lim;
			rat = cpu;
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
}

void
quit(int sig){
	kill(pid, SIGCONT);
	exit(0);
}

void
plimit(pid_t p, int lim)
{
	if(fork() > 0)
		return;

	signal(SIGINT, quit);
	signal(SIGTERM, quit);

	pid = p;
	limit(lim);
}
