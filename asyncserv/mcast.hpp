/**
 *============================================================
 *  @file        mcast.h
 *  @brief      Define the interfaces to send multicast datagram
 * 
 *  compiler   gcc4.1.2
 *  platform   Linux
 *
 *============================================================
 */

#ifndef ASYNC_SERVER_MCAST_H_
#define ASYNC_SERVER_MCAST_H_

#include <time.h>
extern "C" {
#include <glib/ghash.h>
}

enum {
	mcast_notify_addr   = 0,
	mcast_reload_text	= 1
};

enum {
	addr_mcast_1st_pkg	= 1,
	addr_mcast_syn_pkg	= 2
};

struct addr_node_t {
	uint32_t	svr_id;
	char		ip[16];
	uint16_t	port;
	time_t		last_syn_tm;
};

struct addr_cache_t {
	char		svr_name[16];
	GHashTable*	addr_tbl;
};


#pragma pack(1)
struct mcast_pkg_header_t {
	uint16_t	pkg_type;   // for mcast_notify_addr: 1st, syn
	uint16_t	proto_type; // mcast_notify_addr, mcast_reload_text
	char		body[];
};

struct addr_mcast_pkg_t {
	uint32_t	svr_id;
	char		name[16];
	char		ip[16];
	uint16_t	port;
};

struct reload_text_pkg_t {
	char		svr_name[16];
	uint32_t	svr_id;
	char		new_so_name[32];
};
#pragma pack()

class Mcast {
public:
	Mcast();
	~Mcast();

	int create_addr_mcast_socket();
	int asynsvr_create_mcast_socket();
	void send_addr_mcast_pkg(uint32_t pkg_type);
	int send_mcast_pkg(const void* data, int len);
	addr_node_t* get_service_ipport(const char* service, unsigned int svr_id);

	void syn_addr_info();
	void asyncserv_proc_mcast_pkg(void* data, int len);
	
private:
	void del_expired_addrs();
	void proc_addr_mcast_pkg(const mcast_pkg_header_t* hdr, int len);
	void proc_reload_plugin(reload_text_pkg_t* pkg, int len);

private:
	time_t next_syn_addr_tm;
	time_t next_del_addrs_tm;
	
	GHashTable* svr_tbl;
	char addr_buf[sizeof(mcast_pkg_header_t) + sizeof(addr_mcast_pkg_t)];

	// multicast socket for service name synchronization
	int addr_mcast_fd;
	struct sockaddr_storage addr_mcast_addr;
	socklen_t addr_mcast_len;

	// multicast socket
	int mcast_fd;
	struct sockaddr_storage mcast_addr;
	socklen_t mcast_len;
};

extern Mcast g_mcast;

#endif // ASYNC_SERVER_MCAST_H_

