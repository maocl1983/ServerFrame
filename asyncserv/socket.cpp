#include <assert.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <linux/types.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>

extern "C" {
#include <libcommon/log.h>
#include <libcommon/time/time.h>
#include <libcommon/conf_parser/config.h>
#include <libcommon/inet/tcp.h>
}
#include "binding.hpp"
#include "daemon.hpp"
#include "util.hpp"

#include "dll.hpp"
#include "mcast.hpp"
#include "net_if.hpp"
#include "worker.hpp"

#include "socket.hpp"

SocketConn g_sock_conn;

enum {
	trash_size		= 4096,
	mcast_pkg_size	= 8192,
	udp_pkg_size	= 8192
};

int
fd_buff_t::init_recv_buff()
{
	recv_pkg_len = 0;
	recv_len = 0;
	recv_buf = (uint8_t*)mmap(0, page_size, PROT_READ | PROT_WRITE, 
			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (recv_buf == MAP_FAILED) {
		ERROR_LOG("mmap failed");
		return -1;
	}

	swap_recv_buf = (uint8_t*)mmap(0, page_size, PROT_READ | PROT_WRITE, 
			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (swap_recv_buf == MAP_FAILED) {
		ERROR_LOG("mmap failed");
		return -1;
	}
	TRACE_LOG("frame:fd init recv buff");
	return 0;
}

int
fd_buff_t::init_send_buff(uint32_t size)
{
	if (!send_buf) {
		send_buf = (uint8_t*)malloc(size);
		if (!send_buf) {
			ERROR_LOG("malloc error, %s", strerror(errno));
			return -1;
		}
		send_buf_size = size;
	} else if (send_buf_size < send_len + size) {
		send_buf = (uint8_t*)realloc(send_buf, send_len + size);
		if (!send_buf) {
			ERROR_LOG("realloc error, %s", strerror(errno));
			return -1;
		}
		send_buf_size = send_len + size;
	}
	return 0;
}

void
fd_buff_t::destroy()
{
	if (send_buf) {
		free(send_buf);
		send_buf = 0;
	}
	if (recv_buf) {
		munmap(recv_buf, page_size);
		recv_buf = 0;
	}
	if (swap_recv_buf) {
		munmap(swap_recv_buf, page_size);
		swap_recv_buf = 0;
	}
	recv_len = 0;
	send_len = 0;
}


//使得work进程可以支持udp.
//bind udp 端口,然后在proc_udp_pkg 中处理得到的数据
int 
SocketConn::bind_udp_socket( const char* ipaddr, in_port_t port )
{
	int err;
	int listenfd;
	struct sockaddr_in servaddr;
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family  = AF_INET;
	servaddr.sin_port    = htons(port);
	if (ipaddr) {
		inet_pton(AF_INET, ipaddr, &servaddr.sin_addr);
	} else {	
		servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	}

	if ((listenfd = socket(AF_INET,SOCK_DGRAM , 0)) == -1) {
		return -1;
	}

	if (bind(listenfd, (sockaddr*)&servaddr, sizeof(servaddr)) == -1) {
		err = errno;
		close(listenfd);
		errno = err;
		return -1;
	}
	ERROR_LOG("bind_udp_socket:do add conn[%d]",listenfd);
	//return do_add_conn(listenfd, em_fd_type_udp, &servaddr, 0);
	return add_one_conn(listenfd, em_fd_type_udp, &servaddr, 0);
}

bool
SocketConn::is_all_send_over()
{
	for (int i = 0; i <= maxfd; ++i) {
		if ( (fds[i].type == em_fd_type_remote) && (fds[i].buff.send_len > 0) ) {
			return false;
		}
	}
	return true;
}

uint32_t 
SocketConn::get_remote_ip(int fd)
{
	if ((fd >= 0) && (fd <= maxfd) && (fds[fd].type != em_fd_type_unused)) {
		return fds[fd].remote.ip;
	}

	return 0;
}

int
SocketConn::init(int fd_num, int maxevents)
{
	if ((epfd = epoll_create(maxevents)) < 0) {
		ERROR_LOG ("epoll_create failed, %s", strerror (errno));
		return -1;
	}

	evs = (epoll_event*)calloc(maxevents, sizeof(struct epoll_event));
	if (!evs) {
		close(epfd);
		ERROR_LOG("malloc epoll failed, size=%d", maxevents);
		return -1;
	}

	fds = (struct fd_info_t*)calloc(fd_num, sizeof(struct fd_info_t));
	if (!fds) {
		free(evs);
		ERROR_LOG("malloc failed, size=%d", fd_num);
		return -1;
	}

	fd_idx = 0;
	maxfd = 0;
	max_ev_num = maxevents;
	INIT_LIST_HEAD(&epollin_head);
	INIT_LIST_HEAD(&close_head);
	return 0;
}

/*int 
SocketConn::listen(const char *listen_ip, uint16_t listen_port, BindElem* bind_elem)
{
	int ret_code = -1;
	//g_listen_port = listen_port;

	int listenfd = safe_socket_listen(listen_ip, listen_port, SOCK_STREAM, 1024, 32 * 1024);
	if (listenfd != -1) {
		//set nonblock
		set_io_blockability(listenfd, 1);

		ERROR_LOG("net_start:do add conn[%d]",listenfd);
		//do_add_conn(listenfd, em_fd_type_listen, 0, bind_elem);
		add_one_conn(listenfd, em_fd_type_listen, 0, bind_elem);
		ret_code = 0;
	}

	BOOT_LOG(ret_code, "Listen on %s:%u", listen_ip ? listen_ip : "ANYADDR", listen_port);
}*/

int 
SocketConn::send_data(int fd, const void* data, uint32_t len)
{
	bool prev_send_flag = false;
	
	//先清除未发送完毕的
	if (fds[fd].buff.send_len > 0) {
		if (write_to_conn(fd) == -1) {
			ERROR_LOG("fail to write to conn=%d err=%d %s", fd, errno, strerror(errno));
			del_one_conn(fd, is_parent);
			return -1;
		}
		prev_send_flag = true;
	}

	int send_bytes = 0;
	//buff为空了，可以发送data
	if (fds[fd].buff.send_len == 0) {
		send_bytes = safe_tcp_send_n(fd, data, len);
		if (send_bytes == -1) {
			ERROR_LOG("fail to write to fd=%d err=%d %s", fd, errno, strerror(errno));
			del_one_conn(fd, is_parent);
			return -1;
		}
	}

	//将未发送完的合并到buff中
	if (len > (uint32_t)send_bytes){
		fds[fd].buff.init_send_buff(len - send_bytes);
		memcpy(fds[fd].buff.send_buf + fds[fd].buff.send_len, 
				(char*)data + send_bytes, len - send_bytes);
		fds[fd].buff.send_len += len - send_bytes;
		if (is_parent && (g_send_buf_limit_size > 0)
				&& (fds[fd].buff.send_len > g_send_buf_limit_size)) {
			ERROR_LOG("send buf limit exceeded: fd=%d buflen=%u limit=%u",
						fd, fds[fd].buff.send_len, g_send_buf_limit_size);
			del_one_conn(fd, is_parent);
			return -1;
		}
	}

	//更改epoll
	if (fds[fd].buff.send_len > 0 && !prev_send_flag)  {
		mod_events(fd, EPOLLOUT | EPOLLIN);
	} else if (prev_send_flag && fds[fd].buff.send_len == 0) {
		mod_events(fd, EPOLLIN);
	}

	return 0;
}

int 
SocketConn::add_one_conn(int fd, uint8_t type, struct sockaddr_in *peer, BindElem* bind_elem)
{
	//static uint32_t seq = 0;
	uint32_t flag;

	switch (type) {
	case em_fd_type_pipe:
	case em_fd_type_mcast:
	case em_fd_type_addr_mcast:
	case em_fd_type_udp:
		flag = EPOLLIN;
		break;
	case em_fd_type_asyn_connect:
		flag = EPOLLOUT;
		break;
	default:
		flag = EPOLLIN | EPOLLET;
		break;
	}

	if (add_events(fd, flag) == -1) {
		return -1;
	}

	memset(&fds[fd], 0x00, sizeof(fd_info_t));
	fds[fd].sockfd = fd;
	fds[fd].type = type;
	fds[fd].id = ((++fd_idx)==0) ? (++fd_idx) : fd_idx;
	/*fds[fd].id = ++seq;
	if ( seq == 0 ) {
		fds[fd].id = ++seq;
	}*/
	if (peer) {
		fds[fd].remote.ip = peer->sin_addr.s_addr;
		fds[fd].remote.port = peer->sin_port;
	}
	fds[fd].bind_elem = bind_elem;
	maxfd = maxfd > fd ? maxfd : fd;

	TRACE_LOG("add fd=%d, type=%d, id=%u", fd, type, fds[fd].id);
	return 0;
}

void 
SocketConn::del_one_conn(int fd, int process_type)
{
	if (fds[fd].type == em_fd_type_unused)
		return ;

	if (process_type == em_child_type) {
		g_dll.on_fd_closed(fd);
	} else if (process_type == em_parent_type){
		struct shmq_block_t blk;
		blk.id = fds[fd].id;
		blk.fd = fd;
		blk.type = em_close_block;
		blk.len = sizeof(shmq_block_t);
		fds[fd].bind_elem->recvq.push_data(&blk, NULL);
	}

	del_from_epollin_queue(fd);
	del_from_close_queue(fd);

	fds[fd].buff.destroy();
	fds[fd].type = em_fd_type_unused;

	//epoll will auto clear epoll events when fd closed
	close(fd);

	//设置maxfd
	if (maxfd == fd) {
		int i;
		for (i = fd - 1; i >= 0; i--) {
			if (fds[i].type != em_fd_type_unused) {
				break;
			}
		}
		maxfd = i;
	}
	TRACE_LOG ("close fd=%d", fd);
}

int 
SocketConn::open_one_conn(int fd, int process_type)
{
	struct sockaddr_in peer;
	int newfd = safe_tcp_accept(fd, &peer, 1);
	if (newfd != -1) {
		//ERROR_LOG("open_one_conn:do add conn[%d]",newfd);
		add_one_conn(newfd, em_fd_type_remote, &peer, fds[fd].bind_elem);
		fds[newfd].remote.last_tm = get_now_tv()->tv_sec;

		if (process_type == em_parent_type) {
			struct shmq_block_t blk;
			blk.id = fds[newfd].id;
			blk.fd = newfd;
			blk.type = em_open_block;
			blk.len = sizeof(blk) + sizeof(remote_info_t);
			if (fds[newfd].bind_elem->recvq.push_data(&blk, (const uint8_t *)&fds[newfd].remote) == -1) {
				del_one_conn(newfd, 2);
			}
		}
	} else if ((errno == EMFILE) || (errno == ENFILE)) {
		add_to_epollin_queue(fd);
	} else if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
		del_from_epollin_queue(fd);
	}

	//DEBUG_LOG("-----OPEN ONE CONN------fd=%d, process_type=%d", fd, process_type);

	return newfd;
}

int 
SocketConn::write_to_conn(int fd)
{
	int send_bytes;
	
	send_bytes = safe_tcp_send_n(fd, fds[fd].buff.send_buf, fds[fd].buff.send_len);
	if (send_bytes == 0) {
		return 0;
	} else if (send_bytes > 0) {
		if ((uint32_t)send_bytes < fds[fd].buff.send_len) {
			memmove(fds[fd].buff.send_buf, fds[fd].buff.send_buf + send_bytes, 
					fds[fd].buff.send_len - send_bytes);
		}

		fds[fd].buff.send_len -= send_bytes;
		fds[fd].remote.last_tm = get_now_tv()->tv_sec;
	} else {
		ERROR_LOG("failed to write to fd=%d err=%d %s", fd, errno, strerror(errno));
		return -1;
	}

	return send_bytes;
}

int 
SocketConn::read_from_conn(int fd, int max)
{
	int recv_bytes;

	if (!fds[fd].buff.recv_buf && (fds[fd].buff.init_recv_buff()==-1)) {
		return -1;
	}

	if (page_size == (int)fds[fd].buff.recv_len) {
		ERROR_LOG ("recv buffer is full, fd=%d", fd);
		return 0;
	}

	recv_bytes = safe_tcp_recv(fd, fds[fd].buff.recv_buf + fds[fd].buff.recv_len, 
								max - fds[fd].buff.recv_len);
	if (recv_bytes > 0) {
		fds[fd].buff.recv_len += recv_bytes;
		fds[fd].remote.last_tm  = get_now_tv()->tv_sec;
	//close
	} else if (recv_bytes == 0) {
		ERROR_LOG("connection [fd=%d ip=0x%X] closed by peer", fd, fds[fd].remote.ip);
		return -1;
	} else { //EAGAIN ...
		ERROR_LOG("recv error: fd=%d errmsg=%s", fd, strerror(errno));
		recv_bytes = 0;
	}

	if (fds[fd].buff.recv_len == (uint32_t)max) {
		add_to_epollin_queue(fd);
	} else {
		del_from_epollin_queue(fd);
	}

	return recv_bytes;
}

int 
SocketConn::add_events(int fd, uint32_t flag)
{
	struct epoll_event ev;
	ev.events = flag;
	ev.data.fd = fd;
	
	TRACE_LOG("epoll_ctl add %d", fd);
	while(unlikely(epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) != 0)) {
		if (errno != EINTR) {
			ERROR_LOG("epoll ctl add %d error: %m", fd);
			return -1;
		}
	}

	return 0; 
}

int 
SocketConn::mod_events(int fd, uint32_t flag)
{
	struct epoll_event ev;
	ev.events = EPOLLET | flag;
	ev.data.fd = fd;

	while(unlikely(epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev) != 0)) {
		ERROR_LOG ("epoll_ctl mod %d error: %m", fd);
		if (errno != EINTR) {
			return -1;
		}
	}

	return 0;
}

int 
SocketConn::handle_pipe_event(int fd, int pos, int process_type)
{
	char trash[trash_size];

	if (evs[pos].events & EPOLLHUP) {
		if (process_type == em_parent_type) { // Child Crashed
			int pfd = evs[pos].data.fd;
			BindElem* be = fds[pfd].bind_elem;
			CRIT_LOG("CHILD PROCESS CRASHED!\t[olid=%u olname=%s]", be->server_id, be->server_name);

			//关闭与这个子进程关联的链接
			int i;
			for (i = 0; i <= maxfd; ++i) {
				if ((fds[i].bind_elem == be) && (fds[i].type != em_fd_type_listen)) {
					del_one_conn(i, process_type);
				}
			}
			// prevent child process from being restarted again and again forever
			if (be->restart_cnt++ < 20) {
				g_work_svr.restart_process(be);
			}
		} else { // Parent Crashed
			CRIT_LOG("PARENT PROCESS CRASHED!");
			g_daemon.stop_flag = 1;
			return -1;
		}
	} else {
		while (read(fd, trash, trash_size) == trash_size) ;
	}

	return 0;
}

void
SocketConn::handle_send_queue()
{
	struct shmq_block_t *blk;
	int i = 0;
	for ( ; i != g_binds.bind_num; ++i ) {
		ShmqQueue* queue = &(g_binds.configs[i].sendq);
		while (queue->pop_data(&blk) == 0) {
			deal_block_data(blk);	
		}
	}
}

void 
SocketConn::handle_recv_queue()
{
	g_work_svr.deal_recv_queue();
}

void 
SocketConn::del_from_close_queue(int fd)
{
	if (fds[fd].flag & CN_NEED_CLOSE) {
		fds[fd].flag &= ~CN_NEED_CLOSE;
		list_del_init(&fds[fd].list);
	}
}

void 
SocketConn::del_from_epollin_queue(int fd)
{
	if (fds[fd].flag & CN_NEED_POLLIN) {
		fds[fd].flag &= ~CN_NEED_POLLIN;
		list_del_init(&fds[fd].list);
		TRACE_LOG ("del fd=%d from etin queue", fd);
	}
}

void 
SocketConn::add_to_epollin_queue(int fd)
{
	if (!(fds[fd].flag & (CN_NEED_CLOSE | CN_NEED_POLLIN))) {
		list_add_tail(&fds[fd].list, &epollin_head);
		fds[fd].flag |= CN_NEED_POLLIN;
		TRACE_LOG ("add fd=%d to etin queue", fd);
	}
}

void 
SocketConn::add_to_close_queue(int fd)
{
	del_from_epollin_queue(fd);
	if (!(fds[fd].flag & CN_NEED_CLOSE)) {
		list_add_tail(&fds[fd].list, &close_head);
		fds[fd].flag |= CN_NEED_CLOSE;
		TRACE_LOG("add fd=%d to close queue, %x", fd, fds[fd].flag);
	}
}

void 
SocketConn::iterate_close_queue()
{
	struct list_head *l, *p;
	struct fd_info_t *info;

	list_for_each_safe(p, l, &close_head) {
		info = list_entry(p, struct fd_info_t, list);
		if (info->buff.send_len > 0) {
			write_to_conn(info->sockfd);
		}
		del_one_conn(info->sockfd, is_parent ? 2 : 0);
	}
}

void 
SocketConn::iterate_epollin_queue(int max_len, int process_type)
{
	struct list_head *l, *p;
	struct fd_info_t *info;

	list_for_each_safe(p, l, &epollin_head) {
		info = list_entry(p, struct fd_info_t, list);
		if (unlikely(info->type == em_fd_type_listen)) {
			//accept
			while (open_one_conn(info->sockfd, process_type) > 0) ;
		} else if (net_recv(info->sockfd, max_len, process_type) == -1) {
			del_one_conn(info->sockfd, process_type);
		}
	}
}

int 
SocketConn::deal_block_data(struct shmq_block_t *blk)
{
	int data_len;
	int fd = blk->fd;

	if (unlikely((fd > maxfd) || (fd < 0))) {
		DEBUG_LOG("discard the block: blk->type=%d, fd=%d, maxfd=%d, id=%u", 
					blk->type, fd, maxfd, blk->id);
		return -1;
	}

	if (fds[fd].type != em_fd_type_remote || blk->id != fds[fd].id) { 
		TRACE_LOG ("connection %d closed, discard %u, %u block", fd, blk->id, fds[fd].id);
		return -1;
	}

	if (blk->type == em_fin_block && fds[fd].type != em_fd_type_listen) {
		add_to_close_queue (fd);
		return 0;
	}

	//发送
	data_len = blk->len - sizeof (shmq_block_t);
	return net_send(fd, blk->data, data_len);
}

int 
SocketConn::net_recv(int fd, int max, int process_type)
{
	int recv_tm = 0;

	assert(max <= page_size);
	if (fds[fd].type == em_fd_type_pipe) {
		read(fd, fds[fd].buff.recv_buf, max);
		return 0;
	}
	if (read_from_conn(fd, max) == -1) {
		ERROR_LOG("read from conn errror!");
		return -1;
	}

	uint8_t* tmp_recv_buf = fds[fd].buff.recv_buf;
recv_again:
	if (fds[fd].buff.recv_pkg_len == 0) {
		fds[fd].buff.recv_pkg_len = g_dll.get_pkg_len(fd, tmp_recv_buf, fds[fd].buff.recv_len, process_type);
		TRACE_LOG("handle_parse pid=%d return %d, buffer len=%d, fd=%d", getpid(),
					fds[fd].buff.recv_pkg_len, fds[fd].buff.recv_len, fd);
	}

	//长度无效
	if (unlikely(fds[fd].buff.recv_pkg_len > (uint32_t)max)) {
		ERROR_LOG("rece pkg len error[%d %d]!",fds[fd].buff.recv_pkg_len, max);
		return -1;
	} else if (unlikely(fds[fd].buff.recv_pkg_len == 0)) {
		if (fds[fd].buff.recv_len == (uint32_t)max) {
			ERROR_LOG("unsupported protocol, recv_len=%d", fds[fd].buff.recv_len);
			return -1;
		}
	//完整的协议包	
	} else if (fds[fd].buff.recv_len >= fds[fd].buff.recv_pkg_len) {
		if (process_type == em_child_type) {
			g_dll.proc_pkg_from_serv(fd, tmp_recv_buf, fds[fd].buff.recv_pkg_len);
			if (fds[fd].type == em_fd_type_unused) {
				return recv_tm;
			}
		} else {
			struct shmq_block_t blk;
			blk.id = fds[fd].id;
			blk.fd = fd;
			blk.type = em_data_block;
			blk.len	= fds[fd].buff.recv_pkg_len + sizeof(struct shmq_block_t);
			if (fds[fd].bind_elem->recvq.push_data(&blk, tmp_recv_buf)) {
				ERROR_LOG("shmq push data error![fd=%d]", fd);
				return -1;
			}
		}

		recv_tm++;
		//数据没有发送完毕
		if (fds[fd].buff.recv_len > fds[fd].buff.recv_pkg_len) {
			tmp_recv_buf += fds[fd].buff.recv_pkg_len;
		}
		fds[fd].buff.recv_len -= fds[fd].buff.recv_pkg_len;
		fds[fd].buff.recv_pkg_len  = 0;
		if (fds[fd].buff.recv_len > 0) {
			goto recv_again;
		}
	}

	if (fds[fd].buff.recv_buf != tmp_recv_buf) {
		if (fds[fd].buff.recv_len) {
			//TODO：效率不好
			memcpy(fds[fd].buff.swap_recv_buf, tmp_recv_buf, fds[fd].buff.recv_len);
			memcpy(fds[fd].buff.recv_buf, fds[fd].buff.swap_recv_buf, fds[fd].buff.recv_len);
		}
	}

	return recv_tm;
}

int 
SocketConn::net_loop(int timeout, int max_len, int process_type)
{
	iterate_close_queue();
	iterate_epollin_queue(max_len, process_type);

	//nr = epoll_wait(epfd, evs, max_ev_num, timeout);
	int nr = epoll_wait(epfd, evs, max_ev_num, 10);
	if (unlikely(nr < 0 && errno != EINTR)) {
		ERROR_RETURN(("epoll_wait failed, maxfd=%d, epfd=%d: %m", maxfd, epfd), -1);
	}

	renew_now();

	if (process_type == em_parent_type) {
		handle_send_queue();
	}

	for (int pos = 0; pos < nr; pos++) {
		int fd = evs[pos].data.fd;
		
		if (fd > maxfd || fds[fd].sockfd != fd || fds[fd].type == em_fd_type_unused) {
			ERROR_LOG("delayed epoll events: event fd=%d, cache fd=%d, maxfd=%d, type=%d", 
						fd, fds[fd].sockfd, maxfd, fds[fd].type);
			continue;
		}

		if ( unlikely(fds[fd].type == em_fd_type_pipe) ) {
			if (handle_pipe_event(fd, pos, process_type) == 0) {
				continue;
			} else {
				ERROR_LOG("fd type error![fd=%d type=%d]", fd, fds[fd].type);
				return -1;
			}
		}

		/*if ( unlikely(fds[fd].type == em_fd_type_asyn_connect) ) {
			handle_asyn_connect(fd);
			continue;
		}*/

		if (evs[pos].events & EPOLLIN) {
			switch (fds[fd].type) {
			case em_fd_type_listen:
				//accept
				while (open_one_conn(fd, process_type) > 0) ;
				break;
			case em_fd_type_mcast:
			{
				static char buf[mcast_pkg_size];
				int  i;
				for (i = 0; i != 100; ++i) {
					int len = recv(fd, buf, mcast_pkg_size, MSG_DONTWAIT);
					if (len > 0) {
						if (g_dll.proc_mcast_pkg) {
							g_dll.proc_mcast_pkg((void*)buf, len);
						}
					} else {
						break;
					}
				}
				break;
			}
			case em_fd_type_addr_mcast:
			{
				static char buf[mcast_pkg_size];
				int  i;
				for (i = 0; i != 100; ++i) {
					int len = recv(fd, buf, mcast_pkg_size, MSG_DONTWAIT);
					if (len > 0) {
						g_mcast.asyncserv_proc_mcast_pkg(buf, len);
					} else {
						break;
					}
				}
				break;
			}
			case em_fd_type_udp:
			{
				static char buf[udp_pkg_size];
				int  i;
				for (i = 0; i != 100; ++i) {
					struct sockaddr_in from; 
					socklen_t fromlen;
					int len = recvfrom(fd, buf, udp_pkg_size, MSG_DONTWAIT,
										(struct sockaddr*)(&from), &fromlen);
					if (len > 0) {
						g_dll.proc_udp_pkg(fd, buf, len, &from, fromlen);
					} else {
						break;
					}
				}
				break;
			}
	
			default:
				if (net_recv(fd, max_len, process_type) == -1) {
					del_one_conn(fd, process_type);
				}
				break;
			}
		}

		if (evs[pos].events & EPOLLOUT) {
			if (fds[fd].buff.send_len > 0 && write_to_conn(fd) == -1) {
				del_one_conn(fd, process_type);
			}
			if (fds[fd].buff.send_len == 0) {
				mod_events(fd, EPOLLIN);
			}
		}

		if (evs[pos].events & EPOLLHUP) {
			del_one_conn(fd, process_type);
		}
	}

	if (process_type == em_parent_type && socket_timeout) {
		int i;
		for (i = 0; i <= maxfd; ++i) {
			if ((fds[i].type == em_fd_type_remote)
					&& ((get_now_tv()->tv_sec - fds[i].remote.last_tm) >= socket_timeout)) {
				del_one_conn(i, process_type);
			}
		}
	}

	if(process_type == em_child_type) {
		if (g_dll.proc_events) {
			g_dll.proc_events();
		}
		handle_recv_queue();
		
		g_mcast.syn_addr_info();
	}

	return 0;
}

void 
SocketConn::exit()
{
	int i;
	for (i = 0; i < maxfd + 1; i++) {
		if (fds[i].type == em_fd_type_unused) {
			continue;
		}
		fds[i].buff.destroy();
		close (i);
	}

	free(fds);
	free(evs);
	close(epfd);
}

