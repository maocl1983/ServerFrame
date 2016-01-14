/**
 *============================================================
 *  @file      timer.h
 *  @brief     定时器函数，有秒级和毫秒级两种精度的接口。用于设定某一时刻调用某个函数。需要glib支持。
 * 
 *  compiler   gcc4.1.2
 *  platform   Linux
 *
 *============================================================
 */

#ifndef COMMON_TIMER_H_
#define COMMON_TIMER_H_

// Since C89
#include <stdlib.h>
#include <time.h>
// Since C99
#include <stdint.h>
// Posix
#include <sys/time.h>
// Glib
#include <glib.h>
// User-defined
#include <libcommon/list.h>
#include <libcommon/time/time.h>

/**
 * @typedef callback_func_t
 * @brief   回调函数的类型。如果回调函数返回0，则表示定时器到期时要删除该定时器，反之，则不删除。
 */
typedef int (*callback_func_t)(void*, void*);

/**
 * @struct  timer_struct
 * @brief   秒级定时器。
 */
typedef struct timer_struct {
	struct list_head	global_entry;
	struct list_head	owner_entry;
	/*! 定时器的触发时刻，亦即调用回调函数的时间。*/
	struct timeval		tv;
	void*				owner;
	void*				data;
	int					func_indx;
	callback_func_t		function;
}timer_struct_t;

/**
 * @enum   timer_add_mode
 * @brief  定时器添加模式。用于add_event函数。
 * @see    add_event
 */
typedef enum timer_add_mode {
	/*! 添加一个新的定时器。*/
	timer_add_new_timer,
	/*! 替换一个定时器。如果找不到符合条件的定时器，则创建一个新的定时器。*/
	timer_replace_timer,
} timer_add_mode_t;

/**
 * @brief  初始化定时器功能。必须调用了这个函数，才能使用定时器功能。
 * @see    destroy_timer
 */
//void setup_timer();
void setup_timer(struct list_head* t, callback_func_t* cbs, int init_flag);
/**
 * @brief  销毁所有定时器（包括秒级和微秒级的定时器），并释放内存。
 * @see    setup_timer
 */
void destroy_timer();

void scan_timers_list();

/**
 * @brief  扫描定时器列表，调用到期了的定时器的回调函数，并根据回调函数的返回值决定是否需要把该定时器删除掉。
 *         如果回调函数返回0，则表示要删除该定时器，反之，则不删除。必须定期调用该函数才能调用到期了的定时器的回调函数。
 */
void handle_timer();

/**
 * @brief  添加/替换一个秒级定时器，该定时器的到期时间是expire，到期时回调的函数是func。
 * @param  head 链头，新创建的定时器会被插入到该链表中。
 * @param  func 定时器到期时调用的回调函数。
 * @param  owner 传递给回调函数的第一个参数。
 * @param  data 传递给回调函数的第二个参数。
 * @param  expire 定时器到期时间（从Epoch开始的秒数）。
 * @param  flag 指示add_event添加/替换定时器。如果flag==timer_replace_unconditionally，
 *         那么add_event将在head链表中搜索出第一个回调函数==func的定时器，
 *         然后把这个定时器的到期时间修改成expire。如果找不到符合条件的定时器，则新建一个定时器。
 *         建议只有当head链表中所有定时器的回调函数都各不相同的情况下，才使用timer_replace_unconditionally。
 *         注意：绝对不能在定时器的回调函数中修改该定时器的到期时间！
 * @return 指向新添加/替换的秒级定时器的指针。
 * @see    ADD_TIMER_EVENT, REMOVE_TIMER, remove_timers, REMOVE_TIMERS
 */
timer_struct_t* 
//add_timer_event(struct list_head* head, callback_func_t function, void* owner, void* data, uint32_t add_micro);
add_timer_event(struct list_head* head, int cb_index, void* owner, void* data, uint32_t add_micro);

/**
 * @brief  添加/替换一个秒级定时器，该定时器的到期时间是expire，回调函数是register_timer函数根据定时器类型登记。
 * @param  head 链头，新创建的定时器会被插入到该链表中。
 * @param  fidx 定时器类型。
 * @param  owner 传递给回调函数的第一个参数。
 * @param  data 传递给回调函数的第二个参数。
 * @param  expire 定时器到期时间（从Epoch开始的秒数）。
 * @param  flag 指示add_event添加/替换定时器。如果flag==timer_replace_unconditionally，
 *         那么add_event将在head链表中搜索出第一个回调函数==func的定时器，
 *         然后把这个定时器的到期时间修改成expire。如果找不到符合条件的定时器，则新建一个定时器。
 *         建议只有当head链表中所有定时器的回调函数都各不相同的情况下，才使用timer_replace_unconditionally。
 *         注意：绝对不能在定时器的回调函数中修改该定时器的到期时间！
 * @return 指向新添加/替换的秒级定时器的指针。
 * @see    ADD_TIMER_EVENT, REMOVE_TIMER, remove_timers, REMOVE_TIMERS
 */
//timer_struct_t*
//add_event_ex(list_head_t* head, int fidx, void* owner, void* data, time_t expire, timer_add_mode_t flag);

/**
 * @brief  修改秒级定时器tmr的到期时间。注意：绝对不能在定时器的回调函数中修改该定时器的到期时间！
 * @param  tmr 需要修改到期时间的定时器。
 * @param  exptm 将tmr的到期时间修改成exptm（从Epoch开始的秒数）。
 * @see    add_event, ADD_TIMER_EVENT
 */
//void mod_expire_time(timer_struct_t* tmr, time_t exptm);

static inline void
do_remove_timer(timer_struct_t* t, int freed)
{
	if (t->owner_entry.next != 0) {
		list_del(&t->owner_entry);
	}
	if (freed) {
		list_del(&t->global_entry);
		g_slice_free1(sizeof *t, t);
	} else {
		t->function = 0;
		t->func_indx = 0;
	}
}

/**
 * @brief  删除链表head中所有的定时器，并释放内存。用于删除秒级定时器。
 * @param  head 定时器链表的链头。
 * @see    add_event, ADD_TIMER_EVENT
 */
/*static inline void
remove_timers(list_head_t* head)
{
	timer_struct_t *t;
	list_head_t *l, *m;

	list_for_each_safe (l, m, head) {
		t = list_entry (l, timer_struct_t, sprite_list);
		do_remove_timer(t, 0);
	}
}*/

/**
 * @brief 登记定时器类型，将定时器类型id与回调函数的对应关系保存在一个固定大小的数组中
 * @param nbr,定时器的类型；
 * @param cb,回调函数；
 * @return 0，成功；-1，失败。
 */
//int register_timer_callback(int nbr, callback_func_t cb);
int register_timer_callback(int nbr, callback_func_t cb, int max_timer_type);

/**
 * @brief 删除登记过的所有定时器类型
 */
//void unregister_timers_callback();

/**
 * @brief 程序在线加载text.so时，由于定时器回调函数的地址会发生变化，需要更新定时器类型id与回调函数的关系对应表
 */ 
//void refresh_timers_callback();
void refresh_timers_callback();

#endif // COMMON_TIMER_H_
