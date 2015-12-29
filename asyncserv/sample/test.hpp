#ifndef _SERVICE_H_
#define _SERVICE_H_

#include <asyn_serv/net_if.hpp>
//#include "../dll.hpp"

//int  init_service();
//int  fini_service();
//void proc_events();

//int	 get_pkg_len(int fd, const void* pkg, int pkglen, int isparent);

//int  proc_pkg_from_client(void* data, int len, fdsession_t* fdsess);
///void proc_pkg_from_serv(int fd, void* data, int len);

//void on_client_conn_closed(int fd);
//void on_fd_closed(int fd);
//void proc_udp_pkg(int fd,void* data, int len,struct sockaddr_in *from, socklen_t fromlen);

#if 0
struct cli_proto_head_t {
	uint32_t len; /* 协议的长度 */
	uint32_t cmd; /* 协议的命令号 */
	uint32_t user_id; /* 用户的米米号 */
	uint32_t seq_num;/* 序列号 */
	uint32_t ret; /* S->C, 错误码 */
	uint8_t body[]; /* 包体信息 */
};
#endif

#endif
