#include <string.h>

#include "timer.h"

static struct list_head* g_timers_list;

/*enum {
	max_timer_type	= 10000
};*/

/*用于保存定时器回调函数的地址*/
static callback_func_t* g_tcfs;

//static inline timer_struct_t*
//find_event(callback_func_t func);

//static inline timer_struct_t*
//find_event_with_expire(list_head_t* head, callback_func_t function, time_t expire);

void setup_timer(struct list_head* t, callback_func_t* cbs, int init_flag)
{
	g_timers_list = t;
	g_tcfs = cbs;
	
	if (init_flag) {
		renew_now();
		INIT_LIST_HEAD(g_timers_list);
	}
}

void destroy_timer()
{
	list_head_t *l, *p;

	list_for_each_safe(l, p, g_timers_list) {
		timer_struct_t* t = list_entry(l, timer_struct_t, global_entry);
		do_remove_timer(t, 1);
	}	
}

void handle_timer()
{
	static time_t last = 0;

	renew_now();
	if (last != now.tv_sec) {
		last = now.tv_sec;
		scan_timers_list();
	}
}

timer_struct_t*
//add_timer_event(struct list_head* head, callback_func_t function, void* owner, void* data, uint32_t add_micro)
add_timer_event(struct list_head* head, int cb_index, void* owner, void* data, uint32_t add_micro)
{
	renew_now();
	
	timer_struct_t* timer;
	timer = g_slice_alloc(sizeof *timer);
	INIT_LIST_HEAD(&timer->global_entry);
	INIT_LIST_HEAD(&timer->owner_entry);
	timer->function  = g_tcfs[cb_index];
	timer->func_indx = cb_index;
	//timer->function  = function;
	//timer->func_indx = 0;
	timer->owner     = owner;
	timer->data      = data;
	timer->tv		 = *get_now_tv();
	add_timeval(&(timer->tv), add_micro);

	if (head) {
		list_add_tail(&timer->owner_entry, head);
	}
	list_add_tail(&timer->global_entry, g_timers_list);
	return timer;
}

/*timer_struct_t*
add_event_ex(list_head_t* head, int fidx, void* owner, void* data, time_t expire, timer_add_mode_t flag)
{
	if (!tcfs[fidx]) {
		return 0;
	}

	timer_struct_t* timer = add_event(head, tcfs[fidx], owner, data, expire, flag);
	if (timer) {
		timer->func_indx = fidx;
	}
	return timer;
}*/

void scan_timers_list()
{
	list_head_t *l, *p;
	timer_struct_t* t;

	list_for_each_safe(l, p, g_timers_list) {
		t = list_entry(l, timer_struct_t, global_entry);
		if (!(t->function)) {
			do_remove_timer(t, 1);
		} else if (diff_timeval(&(t->tv), &now) < 0) {
			if ( t->function(t->owner, t->data) == 0 ) {
				do_remove_timer(t, 1);
			}
		}
	}
}

/*void mod_expire_time(timer_struct_t* t, time_t expiretime)
{
	if (!t->expire) return;

	time_t diff = expiretime - now.tv_sec;
	int j = 0;

	t->expire = expiretime;
	for ( ; j != (TIMER_VEC_SIZE - 1); ++j ) {
		if ( diff <= vec[j].expire ) break;
	}

	list_del_init(&t->entry);
	list_add_tail(&t->entry, &vec[j].head);
	set_min_exptm(t->expire, j);
}*/

/*----------------------------------------------
  *  inline utilities
  *----------------------------------------------*/
/*static inline void
add_timer(timer_struct_t *t)
{
	int i, diff;

	diff = t->expire - now.tv_sec;
	for (i = 0; i != (TIMER_VEC_SIZE - 1); ++i) {
		if (diff <= vec[i].expire)
			break;
	}

	list_add_tail(&t->entry, &vec[i].head);
	set_min_exptm(t->expire, i);
}*/

/*static inline int
find_min_idx(time_t diff, int max_idx)
{
	while (max_idx && (vec[max_idx - 1].expire >= diff)) {
		--max_idx;
	}
	return max_idx;
}

static inline void
set_min_exptm(time_t exptm, int idx)
{
	if ((exptm < vec[idx].min_expiring_time) || (vec[idx].min_expiring_time == 0)) {
		vec[idx].min_expiring_time = exptm;
	}
}*/

/*static inline timer_struct_t *
find_event(callback_func_t function)
{
	timer_struct_t* t;

	list_for_each_entry(t, &timers_list, global_entry) {
		if (t->function == function)
			return t;
	}

	return NULL;
}*/

/*static inline timer_struct_t *
find_event_with_expire(list_head_t* head, callback_func_t function, time_t expire)
{
	timer_struct_t* t;

	list_for_each_entry(t, head, sprite_list) {
		if (t->function == function && t->expire == expire)
			return t;
	}

	return NULL;
}*/

/*根据定时器的类型ID登记回调函数的地址*/
int register_timer_callback(int nbr, callback_func_t cb, int max_timer_type)
{
	if (nbr <= 0 || nbr >= max_timer_type) {
		return -1;
	}
	if (g_tcfs[nbr]) {
		return -1;
	}
	g_tcfs[nbr] = cb;
	return 0;
}

/*
void unregister_timers_callback()
{
	memset(tcfs, 0, sizeof(tcfs));
}
*/

/*重新加载text.so后，回调函数的地址发生变化，更新已启动的定时器回调函数地址*/
void refresh_timers_callback()
{
	/*int i;
	for (i = 0; i < TIMER_VEC_SIZE; i++) {
		timer_struct_t* t;
		list_for_each_entry (t, &vec[i].head, entry) {
			t->function = tcfs[t->func_indx];
		}	
	}*/
		
	timer_struct_t* t;
	list_for_each_entry (t, g_timers_list, global_entry) {
		t->function = g_tcfs[t->func_indx];
	}	

	/*micro_timer_struct_t* mt;
	list_for_each_entry (mt, &micro_timer, global_entry) {
		mt->function = tcfs[mt->func_indx];
	}*/
}

