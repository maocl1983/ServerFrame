#ifndef ASYNC_SERVER_NET_H_
#define ASYNC_SERVER_NET_H_

// C89
#include <time.h>
// C99
#include <stdint.h>
// Posix
#include <netinet/in.h>
#include <sys/socket.h>
// Linux
#include <sys/epoll.h>
#include <sys/mman.h>

#include <libcommon/list.h>
#include "shmq.hpp"

#define CN_NEED_CLOSE	0x01
#define CN_NEED_POLLIN	0x02

enum fd_type_t {
	em_fd_type_unused = 0,
	em_fd_type_listen,
	em_fd_type_pipe,
	em_fd_type_remote,
	em_fd_type_mcast,
	em_fd_type_addr_mcast,
	em_fd_type_udp,
	em_fd_type_asyn_connect
};

//TODO
struct fd_buff_t {
	uint32_t recv_pkg_len;
	uint32_t recv_len;
	uint8_t* recv_buf;
	uint8_t* swap_recv_buf;
	
	uint32_t send_buf_size;
	uint32_t send_len;
	uint8_t* send_buf;
	
	int init_send_buff(uint32_t size);
	int init_recv_buff();
	void destroy();
};

struct remote_info_t {
	uint32_t ip;
	uint16_t port;
	uint32_t last_tm;
} __attribute__((packed));

class BindElem;
struct fd_info_t {
	uint32_t id;
	int		 sockfd;
	uint8_t	 type;
	uint8_t	 flag;
	
	remote_info_t remote;
	fd_buff_t buff;
	BindElem* bind_elem;

	list_head_t	list;
};

class SocketConn {
public:
	SocketConn(){}
	~SocketConn(){}

	int init(int size, int maxevents);
	int send_data(int fd, const void* data, uint32_t len);
	int write_to_conn(int fd);
	int net_loop(int timeout, int max_len, int process_type);
	int add_one_conn(int fd, uint8_t type, struct sockaddr_in *peer, BindElem* bind_elem);
	void del_one_conn(int fd, int process_type);
	void exit();

	bool is_all_send_over();
	uint32_t get_remote_ip(int fd);
	
	int bind_udp_socket(const char* ipaddr, in_port_t port);
private:
	int add_events(int fd, uint32_t flag);
	int mod_events(int fd, uint32_t flag);
	int read_from_conn(int fd, int max);
	int open_one_conn(int fd, int process_type);
	int net_recv(int fd, int max, int process_type);
	
	int deal_block_data(struct shmq_block_t *blk);
	int handle_pipe_event(int fd, int pos, int process_type);
	//void handle_asyn_connect(int fd);
	void handle_send_queue();
	void handle_recv_queue();
	
	void del_from_close_queue(int fd);
	void del_from_epollin_queue(int fd);
	void add_to_epollin_queue(int fd);
	void add_to_close_queue(int fd);
	void iterate_close_queue();
	void iterate_epollin_queue(int max_len, int process_type);

private:
	int	maxfd;
	int fd_idx;
	fd_info_t* fds;
	
	int	epfd;
	int	max_ev_num;
	struct epoll_event*	evs;

	list_head_t	close_head;
	list_head_t	epollin_head;
};

extern SocketConn g_sock_conn;

#endif // ASYNC_SERVER_NET_H_
