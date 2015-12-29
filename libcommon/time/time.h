/**
 *============================================================
 *  @file      time.h
 *  @brief     和时间相关的函数。部分函数需要glib支持。
 * 
 *  compiler   gcc4.1.2
 *  platform   Linux
 *
 *============================================================
 */

#ifndef LIBCOMMON_TIME_H_
#define LIBCOMMON_TIME_H_

#include <time.h>
#include <sys/time.h>

// export now and tm_cur
extern struct timeval now;
extern struct tm      tm_cur;

/**
 * @brief 对于对实时性要求不会太高的程序，比如摩尔庄园的Online Server这种循环处理客户端发过来的数据包的程序，
 *        可以先调用一次renew_now，然后处理N个数据包，然后再调用一次renew_now，接着再处理N个数据包，如此反复。
 *        这样在处理数据包的函数里就可以直接使用get_now_tv来获取不太精确的当前时间，从而能稍微提升程序的效率。
 * @return 不太精确的当前时间。
 * @see renew_now, get_now_tm
 */
static inline const struct timeval*
get_now_tv()
{
	return &now;
}

/**
 * @brief 对于对实时性要求不会太高的程序，比如摩尔庄园的Online Server这种循环处理客户端发过来的数据包的程序，
 *        可以先调用一次renew_now，然后处理N个数据包，然后再调用一次renew_now，接着再处理N个数据包，如此反复。
 *        这样在处理数据包的函数里就可以直接使用get_now_tm来获取不太精确的当前时间，从而能稍微提升程序的效率。
 * @return 不太精确的当前时间。
 * @see renew_now, get_now_tv
 */
static inline const struct tm*
get_now_tm()
{
	return &tm_cur;
}

/**
 * @brief 更新当前时间。
 * @see get_now_tv, get_now_tm
 */
static inline void
renew_now()
{
	gettimeofday(&now, 0);
	localtime_r(&now.tv_sec, &tm_cur);
}


void add_timeval(struct timeval* tm, int add_micro);

int diff_timeval(const struct timeval* tm1, const struct timeval* tm2);

/**
  * @brief 把`tm_cur`按小时取整点时间。比如`tm_cur`的时间是20081216-16:10:25，则返回的时间是20081216-16:00:00。
  *
  * @param struct tm tm_cur,  需要按小时取整点的时间。
  *
  * @return time_t, 按小时取整点后从Epoch开始的秒数。
  */
static inline time_t
mk_integral_tm_hr(struct tm tm_cur)
{
	tm_cur.tm_min  = 0;
	tm_cur.tm_sec  = 0;

	return mktime(&tm_cur);
}

/**
  * @brief 把`tm_cur`按天取整点时间。需要glib支持。
  *
  * @param struct tm tm_cur,  需要按天取整点的时间。
  * @param int mday,  对`mday`日按天取整点的时间。如果`tm_cur`为当前时间，并且要计算的是当天的按天整点时间，
  *                   可以填tm_cur.tm_mday。取值范围是1 -- 31。
  * @param int month,  根据month的正负，计算`tm_cur` month个月后或者month个月之前，mday日的整点时间。
  *
  * @return time_t, 按天取整点后从Epoch开始的秒数。比如tm_cur是20081216-16:10:25，mday填15，month填1，
  *                 则返回的是20090115-00:00:00转换成从Epoch开始的秒数。
  */
time_t mk_integral_tm_day(struct tm tm_cur, int mday, int month);

#endif // LIBCOMMON_TIME_H_
