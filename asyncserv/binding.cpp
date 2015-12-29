// C89
#include <stdlib.h>
#include <string.h>
// POSIX
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/mman.h>

extern "C" {
#include <libcommon/log.h>
#include <libcommon/conf_parser/config.h>
#include <libcommon/inet/tcp.h>
}

#include "binding.hpp"

BindManage g_binds;

void 
BindElem::init_shmq()
{
	sendq.create();
	recvq.create();
}

void
BindElem::close_shmq(int is_parent_flag)
{
	if (!is_parent_flag) {
		sendq.close_pipe(0);
		recvq.close_pipe(1);
	} else {
		sendq.close_pipe(1);
		recvq.close_pipe(0);
	}
}

void
BindElem::destroy_shmq()
{
	sendq.destroy();
	recvq.destroy();
}

int
BindElem::add_pipe_conn(int pipe_type)
{
	if (pipe_type == em_send_pipe) {
		g_sock_conn.add_one_conn(sendq.get_pipe_handle(0), em_fd_type_pipe, 0, this);
	} else if (pipe_type == em_recv_pipe) {
		g_sock_conn.add_one_conn(recvq.get_pipe_handle(0), em_fd_type_pipe, 0, 0);
	}

	return 0;
}


int
BindElem::add_listen_conn()
{
	int ret_code = -1;

	int listenfd = safe_socket_listen(bind_ip, bind_port, SOCK_STREAM, 1024, 32 * 1024);
	if (listenfd != -1) {
		//set nonblock
		set_io_blockability(listenfd, 1);

		int tcp_nodelay = config_get_intval("tcp_nodelay", 0);
		if (tcp_nodelay) {
			if (set_tcp_nodelay(listenfd, tcp_nodelay) != 0) {
				ret_code = -2;
				goto listen_end;
			}
		}

		DEBUG_LOG("tcp_nodelay[%d]",tcp_nodelay);
		//ERROR_LOG("start_net:do add conn[%d]",listenfd);
		g_sock_conn.add_one_conn(listenfd, em_fd_type_listen, 0, this);
		ret_code = 0;
	}

listen_end:
	BOOT_LOG(ret_code, "Listen on %s:%u", (bind_ip ? bind_ip : "ANYADDR"), bind_port);
}


BindManage::BindManage()
{
	bind_num = 0;
}

BindManage::~BindManage()
{

}

/**
 * load_bind_file - parse bind config file @file_name
 * 
 * return: 0 on success, otherwise -1
 */
int 
BindManage::load_bind_file(const char* file_name)
{
	const int cn_max_field_num = 4;
	int	ret_code = -1;
	char* buf;

	if (mmap_config_file(file_name, &buf) > 0 ) {
		char* start = buf;
		char* end;
		char* field[cn_max_field_num];

		size_t len = strlen(buf);
		while (buf + len > start) {
			end = strchr(start, '\n');
			if ( end && *end ) {
				*end = '\0';
			}
			if ( (*start != '#') && (str_split(0, start, field, cn_max_field_num) == cn_max_field_num) ) {
				BindElem* bc = &(configs[bind_num]);
				bc->server_id = atoi(field[0]); // server id
				strncpy(bc->server_name, field[1], sizeof(bc->server_name) - 1); // server name
				strncpy(bc->bind_ip, field[2], sizeof(bc->bind_ip) - 1); // server ip
				bc->bind_port = atoi(field[3]); // server port
				// increase bind_num
				++(bind_num);
			}
			start = end + 1;

			if (bind_num > max_listen_fds) {
				BOOT_LOG(ret_code, "bind num larger than max fds[%d %d]:%s", bind_num, max_listen_fds, file_name);
			}
		}

		munmap(buf, len);
		ret_code = 0;
	}

	BOOT_LOG(ret_code, "load bind file:%s", file_name);
}

void 
BindManage::close_shmq_pipes(int idx, int is_parent_flag)
{
	for (int i = 0; i < idx; i++) {
		configs[i].close_shmq(is_parent_flag);
	}
}

void
BindManage::destroy_all_shmq()
{
	for (int i = 0; i < bind_num; i++) {
		configs[i].destroy_shmq();
	}
}

void 
BindManage::destroy_all_shmq_excepet_one(BindElem* bc_elem)
{
	for (int i = 0; i < bind_num; i++) {
		BindElem* elem = &(configs[i]);
		if (bc_elem != elem) {
			elem->destroy_shmq();
		}
	}
}

