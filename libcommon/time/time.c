#include <assert.h>

#include <glib.h>

#include "time.h"

struct timeval  now;
struct tm       tm_cur;

void add_timeval(struct timeval* tm, int add_micro)
{
	tm->tv_sec += add_micro / 1000;
	tm->tv_usec += (add_micro % 1000) * 1000;
	if (tm->tv_usec > 999999) {
		tm->tv_sec += tm->tv_usec / 1000000;
		tm->tv_usec = tm->tv_usec % 1000000;
	}
}

int diff_timeval(const struct timeval* tm1, const struct timeval* tm2)
{
	if (tm1->tv_sec > tm2->tv_sec) {
		return 1;
	} else if (tm1->tv_sec < tm2->tv_sec) {
		return -1;
	} else if (tm1->tv_sec == tm2->tv_sec) {
		if (tm1->tv_usec > tm2->tv_usec) {
			return 1;
		} else if (tm1->tv_usec < tm2->tv_usec) {
			return -1;
		} else if (tm1->tv_usec == tm2->tv_usec) {
			return 0;
		}
	}

	return 0;
}

time_t mk_integral_tm_day(struct tm tm_cur, int mday, int month)
{
	assert( (mday > 0) && (mday < 32) );

	tm_cur.tm_hour = 0;
	tm_cur.tm_min  = 0;
	tm_cur.tm_sec  = 0;

	GDate* gdate1 = g_date_new_dmy(tm_cur.tm_mday, tm_cur.tm_mon + 1, tm_cur.tm_year);
	GDate* gdate2 = g_date_new_dmy(mday, tm_cur.tm_mon + 1, tm_cur.tm_year);

	if (month > 0) {
		g_date_add_months(gdate2, month);
	} else if (month < 0) {
		g_date_subtract_months(gdate2, -month);
	}

	int days = g_date_days_between(gdate1, gdate2);

	g_date_free(gdate1);
	g_date_free(gdate2);
	return mktime(&tm_cur) + days * 86400;
}
