#ifndef ASYNC_SERVER_WORKER_H_
#define ASYNC_SERVER_WORKER_H_

#include <time.h>

extern "C" {
#include <glib/ghash.h>
}

#include "binding.hpp"
#include "net_if.hpp"

struct fd_hashlist_t {
	int count;
	GHashTable*	list;
};

extern int is_parent;

class Worker {
public:
	Worker();
	~Worker();
	
	const BindElem* get_config()
	{ return my_bind_elem; }

	void run_child_process(int my_elem_idx, int inherit_idx);
	void restart_process(BindElem* bind_elem);
	fdsession_t* get_fdsess(int fd);
	void remove_fdsess(int fd);
	int push_block_data(shmq_block_t* blk, const void* data);
	void deal_recv_queue();
private:
	void add_fdsess(fdsession_t* fdsess);

	int handle_init();
	int handle_fini();

	int deal_open_block(const shmq_block_t* blk);
	int deal_close_block(int fd);
	void deal_data_block(uint8_t* recvbuf, int rcvlen, int fd);
private:
	BindElem* my_bind_elem;
	fd_hashlist_t fds;
};

extern Worker g_work_svr;

#endif // ASYNC_SERVER_WORKER_H_
