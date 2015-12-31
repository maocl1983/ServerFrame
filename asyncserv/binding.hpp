#ifndef ASYNC_SERVER_BIND_CONF_H_
#define ASYNC_SERVER_BIND_CONF_H_

// POSIX
#include <netinet/in.h>
// Self-define
#include "shmq.hpp"

enum {
	max_listen_fds	= 200
};

enum process_type_t {
	em_child_type = 0,
	em_parent_type = 1,
};

enum pipe_type_t {
	em_send_pipe = 0,
	em_recv_pipe = 1,
};

class BindElem {
public:
	BindElem(){}
	~BindElem(){}

	void init_shmq();
	void close_shmq(int is_parent_flag);
	void destroy_shmq();

	int add_pipe_conn(int pipe_type);
	int add_listen_conn();
public:
	uint32_t	server_id;
	char		server_name[16];
	char		bind_ip[16];
	in_port_t	bind_port;
	in_port_t	bind_udp_port;
	uint8_t		restart_cnt;
	ShmqQueue	sendq;
	ShmqQueue	recvq;
};

class BindManage {
public:
	BindManage();
	~BindManage();

	int load_bind_file(const char* file_name);
	
	inline int get_bind_conf_idx(const BindElem* bc_elem)
		{return (bc_elem - &(configs[0]));}

	void close_shmq_pipes(int idx, int is_parent_flag);
	void destroy_all_shmq();
	void destroy_all_shmq_excepet_one(BindElem* bc_elem);
//private:
public:
	int bind_num;
	BindElem configs[max_listen_fds];
};

extern BindManage g_binds;




#endif // ASYNC_SERVER_BIND_CONF_H_
