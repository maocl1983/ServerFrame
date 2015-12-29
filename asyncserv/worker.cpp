#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>

#include <glib.h>

extern "C" {
#include <libcommon/log.h>
#include <libcommon/time/time.h>
#include <libcommon/conf_parser/config.h>
}
#include "daemon.hpp"
#include "shmq.hpp"
#include "dll.hpp"
#include "mcast.hpp"
#include "util.hpp"

#include "worker.hpp"
#include "net_if.hpp"

int	is_parent = 1;
Worker g_work_svr;

static inline void
free_fdsess(void* fdsess)
{
	g_slice_free1(sizeof(fdsession_t), fdsess);
}

Worker::Worker()
{
	my_bind_elem = 0;
}
	
Worker::~Worker()
{
	my_bind_elem = 0;
}

fdsession_t*
Worker::get_fdsess(int fd)
{
	return (fdsession_t*)g_hash_table_lookup(fds.list, &fd);
}

void
Worker::add_fdsess(fdsession_t* fdsess)
{
	g_hash_table_insert(fds.list, &(fdsess->fd), fdsess);
	++(fds.count);
}

void
Worker::remove_fdsess(int fd)
{
	g_hash_table_remove(fds.list, &fd);
	--(fds.count);
}

int
Worker::push_block_data(shmq_block_t* blk, const void* data)
{
	return my_bind_elem->sendq.push_data(blk,data);
}

int
Worker::handle_init()
{
	fds.list = g_hash_table_new_full(g_int_hash, g_int_equal, 0, free_fdsess);
	
	//初始网络设置
	g_sock_conn.init(g_daemon.max_fd_num, 2000);
	my_bind_elem->add_pipe_conn(em_recv_pipe);
	TRACE_LOG("service.cpp:do add conn[%d]",my_bind_elem->recvq.get_pipe_handle(0));

	//组播设置
	if (g_dll.proc_mcast_pkg && (g_mcast.asynsvr_create_mcast_socket() == -1)) {
		return -1;
	}

	//TODO:
	if (config_get_strval("addr_mcast_ip")) {
		if (g_mcast.create_addr_mcast_socket() == 0) {
			g_mcast.send_addr_mcast_pkg(addr_mcast_1st_pkg);
		} else {
			return -1;
		}
	}

	return (g_dll.init_service ? g_dll.init_service(0) : 0);	
}

int
Worker::handle_fini()
{
	if (!g_sock_conn.is_all_send_over()) {
		return 0;
	}

	if ( g_dll.fini_service && (g_dll.fini_service(0) != 0) ) {
		return 0;
	}

	g_hash_table_destroy(fds.list);
	return 1;
}

int
Worker::deal_open_block(const shmq_block_t* blk)
{
	fdsession_t* fdsess = get_fdsess(blk->fd);
	if (fdsess || (blk->len != (sizeof(shmq_block_t) + sizeof(remote_info_t)))) {
		ERROR_LOG("deal_open_block , fd=%d length=%d", blk->fd, blk->len);
		return -1;
	} else {
		fdsess = (fdsession_t*)g_slice_alloc(sizeof *fdsess);
		fdsess->fd = blk->fd;
		fdsess->id = blk->id;
		fdsess->remote_port = *(uint16_t*)blk->data;
		fdsess->remote_ip = *(uint32_t*)&blk->data[2];
		add_fdsess(fdsess);
	}

	return 0;
}

void
Worker::deal_data_block(uint8_t* recvbuf, int rcvlen, int fd)
{
	fdsession_t* fdsess = get_fdsess(fd);
	if (fdsess) {
		if (g_dll.proc_pkg_from_client(recvbuf, rcvlen, fdsess)) {
			close_client_conn(fd);
		}
	}
}

void 
Worker::deal_recv_queue()
{
	ShmqQueue* recvq = &(my_bind_elem->recvq);
	ShmqQueue* sendq = &(my_bind_elem->sendq);

	struct shmq_block_t* blk;
	while (recvq->pop_data(&blk) == 0) {
		switch (blk->type) {
		case em_open_block:
			if (deal_open_block(blk) == -1) {
				blk->type = em_fin_block;
				blk->len = sizeof(*blk);
				sendq->push_data(blk, NULL);
			}
			break;
		case em_close_block:
			deal_close_block(blk->fd);
			break;		
		case em_data_block:
			if (blk->len > sizeof(*blk)) {
				deal_data_block(blk->data, blk->len - sizeof(*blk), blk->fd);
			}
			break;
		default:
			break;
		}
	}
}

int 
Worker::deal_close_block(int fd)
{
	fdsession_t* fdsess = get_fdsess(fd);
	if (!fdsess) {
		ERROR_RETURN( ("connection %d had already been closed", fd), -1 );
	}
	assert(fds.count > 0);

	g_dll.on_client_conn_closed(fd);
	remove_fdsess(fd);
	return 0;
}

void 
Worker::run_child_process(int my_elem_idx, int inherit_idx)
{
	//初始化子进程
	is_parent = 0;
	my_bind_elem = &(g_binds.configs[my_elem_idx]);
    int timeout = config_get_intval("net_loop_interval", 100);
	init_logfile(my_bind_elem->server_id);

	//释放从父进程上继承下来的资源
	g_binds.close_shmq_pipes(inherit_idx, em_child_type);
	g_binds.destroy_all_shmq_excepet_one(my_bind_elem);
	g_sock_conn.exit();

	g_daemon.set_proc_title("%s-%u", g_daemon.prog_name, my_bind_elem->server_id);

	if (handle_init() != 0 ) {
		ERROR_LOG("fail to init worker process. olid=%u olname=%s", 
				my_bind_elem->server_id, my_bind_elem->server_name);
		goto fail;
	}

    if (timeout < 0 || timeout > 1000) {
        timeout = 100;//用户自定义业务进程的netloop时间
	}

	while ( !g_daemon.stop_flag || !handle_fini() ) {
		g_sock_conn.net_loop(timeout, page_size, 0);
	}

fail:
	my_bind_elem->destroy_shmq();
	g_sock_conn.exit();
	g_dll.unregister_plugin();
	g_daemon.fini_free();

	exit(0);
}

void 
Worker::restart_process(BindElem* bind_elem)
{
	//TODO:
	close(bind_elem->recvq.get_pipe_handle(1));
	g_sock_conn.del_one_conn(bind_elem->sendq.get_pipe_handle(0), 2);
	bind_elem->destroy_shmq();
	bind_elem->init_shmq();

	int i = g_binds.get_bind_conf_idx(bind_elem);
	pid_t pid;

	if ( (pid = fork ()) < 0 ) {
		CRIT_LOG("fork failed: %s", strerror(errno));
	} else if (pid > 0) { //parent process
		bind_elem->close_shmq(em_parent_type);
		bind_elem->add_pipe_conn(em_send_pipe);
		g_daemon.set_child_pid(i,pid);
		TRACE_LOG("service.cpp:do add conn[%d]",bind_elem->recvq.get_pipe_handle(0));
	} else { //child process
		run_child_process(i, g_binds.bind_num);
	}
}

