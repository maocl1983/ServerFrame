#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <sys/socket.h>

extern "C" {
#include <libcommon/log.h>
#include <libcommon/time/time.h>
#include <libcommon/inet/tcp.h>
}
#include "net_if.hpp"
#include "mcast.hpp"
#include "socket.hpp"
#include "dll.hpp"


time_t socket_timeout;
int page_size;
uint32_t g_send_buf_limit_size;
static int g_ip_resolved;
static ip_port_t g_ip_port;

int connect_to_svr(const char* ipaddr, in_addr_t port, int bufsz, int timeout)
{
	struct sockaddr_in peer;
	int fd;

	memset(&peer, 0, sizeof(peer));
	peer.sin_family  = AF_INET;
	peer.sin_port    = htons(port);
	if (inet_pton(AF_INET, ipaddr, &peer.sin_addr) <= 0) {
		ERROR_RETURN(("inet_pton %s failed, %m", ipaddr), -1);
	}

	fd = safe_tcp_connect(ipaddr, port, timeout, 1);
	if (fd != -1) {
		DEBUG_LOG("CONNECTED TO\t[%s:%u fd=%d]", ipaddr, port, fd);
		g_sock_conn.add_one_conn(fd, em_fd_type_remote, &peer, 0);
	} else {
		ERROR_LOG("failed to connect to %s:%u, err=%d %s", ipaddr, port, errno, strerror(errno));
	}

	return fd;
}

int connect_to_service(const char* service_name, uint32_t svr_id, int bufsz, int timeout)
{
	INFO_LOG("TRY CONNECTING TO\t[name=%s id=%u]", service_name, svr_id);

	addr_node_t* n = g_mcast.get_service_ipport(service_name, svr_id);
	if (n) {
		INFO_LOG("SERVICE RESOLVED\t[name=%s id=%u %u ip=%s port=%d]",
					service_name, svr_id, n->svr_id, n->ip, n->port);
		int fd = connect_to_svr(n->ip, n->port, bufsz, timeout);
		if (fd != -1) {
			INFO_LOG("CONNECTED TO\t[%s:%u fd=%d]", n->ip, n->port, fd);
		}

		// for get_last_connecting_service
		g_ip_resolved = 1;
		memcpy(g_ip_port.ip, n->ip, sizeof(g_ip_port.ip));
		g_ip_port.port = n->port;

		return fd;
	}

	g_ip_resolved = 0;
	ERROR_LOG("no server with the name [%s] and server id [%u] is found", service_name, svr_id);
	return -1;
}

void close_svr(int svrfd)
{
	g_sock_conn.del_one_conn(svrfd, 2);
}

int create_udp_socket(struct sockaddr_in* addr, const char* ip, in_port_t port)
{
	memset(addr, 0, sizeof(*addr));

	addr->sin_family = AF_INET;
	addr->sin_port   = htons(port);
	if (inet_pton(AF_INET, ip, &(addr->sin_addr)) <= 0 ) {
		return -1;
	}

	return socket(PF_INET, SOCK_DGRAM, 0);
}

const char* resolve_service_name(const char* service_name, uint32_t svr_id)
{
	addr_node_t* n = g_mcast.get_service_ipport(service_name, svr_id);
	if (n) {
		return n->ip;
	}

	return 0;
}

const ip_port_t* get_last_connecting_service()
{
	if (g_ip_resolved) {
		return &g_ip_port;
	}

	return 0;
}

int net_send(int fd, const void* data, uint32_t len)
{
	return g_sock_conn.send_data(fd, data, len);
}

int send_pkg_to_client(fdsession_t* fdsess, const void* pkg, const int pkglen)
{
	shmq_block_t mb;

	mb.id   = fdsess->id;
	mb.fd   = fdsess->fd;
	mb.type = em_data_block;
	
    int send_bytes, cur_len;
	for (send_bytes = 0; send_bytes < pkglen; send_bytes += cur_len) {
		if ((pkglen - send_bytes) > (page_size - (int)sizeof(shmq_block_t))) {
			cur_len = page_size - sizeof(shmq_block_t);
		} else {
			cur_len = pkglen - send_bytes;
		}

		mb.len = cur_len + sizeof(shmq_block_t);

		if (g_work_svr.push_block_data(&mb, (char*)pkg+send_bytes) == -1) {
			return -1;
		}
	}

	return 0;	
}

void close_client_conn(int fd)
{
	shmq_block_t mb;

	fdsession_t* fdsess = g_work_svr.get_fdsess(fd);
	if (!fdsess) {
		return;
	}

	mb.id = fdsess->id;
	mb.len = sizeof(shmq_block_t);
	mb.type = em_fin_block;
	mb.fd = fd;

	//handle_close(fd);//TODO:
	g_dll.on_client_conn_closed(fd);
	g_work_svr.remove_fdsess(fd);

	g_work_svr.push_block_data(&mb, 0);
}

uint32_t get_remote_ip(int fd)
{
	return g_sock_conn.get_remote_ip(fd);
}

uint32_t get_server_id()
{
	return g_work_svr.get_config()->server_id;
}

const char* get_server_name()
{
	return g_work_svr.get_config()->server_name;
}

const char* get_server_ip()
{
	return g_work_svr.get_config()->bind_ip;
}

in_port_t get_server_port()
{
	return g_work_svr.get_config()->bind_port;
}

uint32_t get_client_ip(const fdsession_t* fdsess)
{
	return fdsess->remote_ip;
}

uint32_t get_client_port(const fdsession_t* fdsess)
{
	return fdsess->remote_port;
}

int send_mcast_pkg(const void* data, int len)
{
	return g_mcast.send_mcast_pkg(data, len);
}


