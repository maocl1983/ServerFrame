/**
 *============================================================
 *  @file        net_if.h
 *  @brief      Essential net interface to deal with network
 * 
 *  compiler   gcc4.1.2
 *  platform   Linux
 *
 *============================================================
 */

#ifndef ASYNC_SERVER_NET_INTERFACE_H_
#define ASYNC_SERVER_NET_INTERFACE_H_

#include <stdint.h>
#include <netinet/in.h>

extern time_t socket_timeout;
extern int page_size;
extern uint32_t	g_send_buf_limit_size;
/**
  * @brief hold an ip and a port
  */
struct ip_port_t {
	/*! ip */
	char		ip[16];
	/*! port */
	in_addr_t	port;
};

typedef struct fdsession {
	int			fd;
	uint32_t	id;
	uint16_t	remote_port;
	uint32_t	remote_ip;
} fdsession_t;

int connect_to_svr(const char* ipaddr, in_addr_t port, int bufsz, int timeout);

int connect_to_service(const char* service_name, uint32_t svr_id, int bufsz, int timeout);

void close_svr(int svrfd);

int create_udp_socket(struct sockaddr_in* addr, const char* ip, in_port_t port);

const char* resolve_service_name(const char* service_name, uint32_t svr_id);

const ip_port_t* get_last_connecting_service();

int net_send(int fd, const void* data, uint32_t len);

int send_pkg_to_client(fdsession_t* fdsess, const void* pkg, const int pkglen);

void close_client_conn(int fd);

uint32_t get_remote_ip(int fd);

uint32_t get_server_id();

const char* get_server_name();

const char* get_server_ip();

in_port_t get_server_port();

uint32_t get_client_ip(const fdsession_t* fdsess);

uint32_t get_client_port(const fdsession_t* fdsess);

int send_mcast_pkg(const void* data, int len);

#endif // ASYNC_SERVER_NET_INTERFACE_H_

