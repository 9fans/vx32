#ifndef _TIMEB_H_
#define _TIMEB_H_

struct timeb
{
	time_t time;
	unsigned short millitm;
	short timezone;
	short dstflag;
};

int ftime(struct timeb*);

#endif  // _TIMEB_H_
