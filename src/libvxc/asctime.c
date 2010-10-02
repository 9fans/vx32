#include <time.h>
#include <string.h>

static char *day[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static char *mon[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

char *asctime(const struct tm *tm)
{
	static char buf[20];
	char *p;
	
	p = buf;
	memmove(p, day[tm->tm_wday], 3);
	p += 3;
	*p++ = ' ';
	memmove(p, mon[tm->tm_mon], 3);
	p += 3;
	*p++ = ' ';
	*p++ = '0' + tm->tm_mday / 10;
	*p++ = '0' + tm->tm_mday % 10;
	*p++ = ' ';
	*p++ = '0' + tm->tm_hour / 10;
	*p++ = '0' + tm->tm_hour % 10;
	*p++ = ':';
	*p++ = '0' + tm->tm_min / 10;
	*p++ = '0' + tm->tm_min % 10;
	*p++ = ':';
	*p++ = '0' + tm->tm_sec / 10;
	*p++ = '0' + tm->tm_sec % 10;
	*p++ = ' ';
	int y = tm->tm_year + 1900;
	*p++ = '0' + y / 1000;
	*p++ = '0' + (y / 100) % 10;
	*p++ = '0' + (y / 10) % 10;
	*p++ = '0' + y % 10;
	*p++ = '\n';
	*p = 0;
	
	return buf;
}
