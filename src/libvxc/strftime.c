#include <stdio.h>
#include <time.h>
#include <string.h>

static char *weekday[] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
static char *wday[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static char *month[] = { "January", "February", "March", "April", "May", "June", "July", "August", "September",
	"October", "November", "December" };
static char *mon[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };


size_t strftime(char  *buf, size_t maxsize, const char *fmt, const struct tm *tm)
{
	char *ep = buf + maxsize - 1;
	char *p = buf;
	int c;
	char tmp[40];
	
	while (*fmt) {
		if ((c = *fmt++) != '%') {
			if (p >= ep)
				return 0;
			*p++ = c;
			continue;
		}
		
		switch (c = *fmt++) {
		case 0:
			return 0;
		
		default:
			if (p+1 >= ep)
				return 0;
			*p++ = '%';
			*p++ = c;
			break;
		
		case 'A':
			strcpy(tmp, weekday[tm->tm_wday]);
			break;
		case 'a':
			strcpy(tmp, wday[tm->tm_wday]);
			break;
		case 'B':
			strcpy(tmp, month[tm->tm_mon]);
			break;
		case 'h':
		case 'b':
			strcpy(tmp, mon[tm->tm_mon]);
			break;
		case 'C':
			sprintf(tmp, "%02d", (tm->tm_year+1900)/100);
			break;
		case 'c':
			sprintf(tmp, "%02d:%02d:%02d %02d/%02d/%04d",
				tm->tm_hour, tm->tm_min, tm->tm_sec,
				tm->tm_mon+1, tm->tm_mday, tm->tm_year+1900);
			break;
		case 'D':
			strftime(tmp, sizeof tmp, "%m/%d/%y", tm);
			break;
		case 'd':
			sprintf(tmp, "%02d", tm->tm_mday);
			break;
		case 'e':
			sprintf(tmp, "%2d", tm->tm_mday);
			break;
		case 'F':
			strftime(tmp, sizeof tmp, "%Y-%m-%d", tm);
			break;
		case 'G':
			// Not quite right - sometimes the previous or next year!
			sprintf(tmp, "%04d", tm->tm_year + 1900);
			break;
		case 'g':
			strftime(tmp, sizeof tmp, "%G", tm);
			memmove(tmp, tmp+2, strlen(tmp+2)+1);
			break;
		case 'H':
			sprintf(tmp, "%02d", tm->tm_hour);
			break;
		case 'I':
			sprintf(tmp, "%02d", (tm->tm_hour+11) % 12 + 1);
			break;
		case 'j':
			sprintf(tmp, "%03d", tm->tm_yday + 1);
			break;
		case 'k':
			sprintf(tmp, "%2d", tm->tm_hour);
			break;
		case 'l':
			sprintf(tmp, "%2d", (tm->tm_hour+11) % 12 + 1);
			break;
		case 'M':
			sprintf(tmp, "%02d", tm->tm_min);
			break;
		case 'm':
			sprintf(tmp, "%02d", tm->tm_mon+1);
			break;
		case 'n':
			strcpy(tmp, "\n");
			break;
		case 'p':
			if (tm->tm_hour < 12)
				strcpy(tmp, "am");
			else
				strcpy(tmp, "pm");
			break;
		case 'R':
			strftime(tmp, sizeof tmp, "%H:%M", tm);
			break;
		case 'r':
			strftime(tmp, sizeof tmp, "%I:%M:%S %p", tm);
			break;
		case 'S':
			sprintf(tmp, "%02d", tm->tm_sec);
			break;
		case 's':
			sprintf(tmp, "%d", mktime(tm));
			break;
		case 'T':
			strftime(tmp, sizeof tmp, "%H:%M:%S", tm);
			break;
		case 't':
			strcpy(tmp, "\t");
			break;
		case 'U':
			sprintf(tmp, "%02d", tm->tm_yday/7);  // not quite right
			break;
		case 'u':
			sprintf(tmp, "%d", tm->tm_wday ? tm->tm_wday : 7);
			break;
		case 'V':
			sprintf(tmp, "%02d", 1 + tm->tm_yday/7);  // not quite right
			break;
		case 'v':
			strftime(tmp, sizeof tmp, "%e-%b-%Y", tm);
			break;
		case 'W':
			sprintf(tmp, "%02d", tm->tm_yday/7);  // not quite right
			break;
		case 'w':
			sprintf(tmp, "%d", tm->tm_wday);
			break;
		case 'X':
			sprintf(tmp, "%02d:%02d:%02d",
				tm->tm_hour, tm->tm_min, tm->tm_sec);
			break;
		case 'x':
			sprintf(tmp, "%%02d/%02d/%04d",
				tm->tm_mon+1, tm->tm_mday, tm->tm_year+1900);
			break;
		case 'Y':
			sprintf(tmp, "%d", tm->tm_year+1900);
			break;
		case 'y':
			sprintf(tmp, "%02d", tm->tm_year%100);
			break;
		case 'Z':
			strcpy(tmp, "GMT");
			break;
		case 'z':
			strcpy(tmp, "+0000");
			break;
		case '%':
			strcpy(tmp, "%");
			break;
		}
		if(p+strlen(tmp) > ep)
			return 0;
		strcpy(p, tmp);
		p += strlen(p);
	}
	*p = 0;
	return p - buf;
}

