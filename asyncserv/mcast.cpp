#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <net/if.h>
#include <arpa/inet.h>

#include <glib.h>

extern "C" {
#include <libcommon/log.h>
#include <libcommon/time/time.h>
#include <libcommon/conf_parser/config.h>
#include <libcommon/inet/mcast.h>
#include <libcommon/random/random.h>
}
#include "dll.hpp"
#include "net_if.hpp"
#include "socket.hpp"
#include "util.hpp"

#include "mcast.hpp"

Mcast g_mcast;

static gboolean addr_pred(gpointer key, gpointer value, gpointer user_data)
{
	int* n = (int*)user_data;
	if (*n == 0) {
		return TRUE;
	} else {
		(*n)--;
		return FALSE;
	}
}

static gboolean del_an_expired_addr(gpointer key, gpointer value, gpointer user_data)
{
	addr_node_t* n = (addr_node_t*)value;
	if ( (get_now_tv()->tv_sec - n->last_syn_tm) > 100 ) {
		/*if (dll.sync_service_info) {
			dll.sync_service_info(n->svr_id, (char*)user_data, n->ip, n->port, 0);
		}*/

		INFO_LOG("DEL AN ADDR\t[id=%u ip=%s port=%d last_tm=%ld]",
					n->svr_id, n->ip, n->port, n->last_syn_tm);
		return TRUE;
	}
	return FALSE;
}

static void do_del_expired_addrs(gpointer key, gpointer value, gpointer user_data)
{
	addr_cache_t* ac = (addr_cache_t*)value;
	g_hash_table_foreach_remove(ac->addr_tbl, del_an_expired_addr, ac->svr_name);
}

static void free_addr_cache(void* addr_cache)
{
	addr_cache_t* addr_c = (addr_cache_t*)addr_cache;
	g_hash_table_destroy(addr_c->addr_tbl);
	g_slice_free1(sizeof(*addr_c), addr_c);
}

static void free_addr_node(void* addr_node)
{
	g_slice_free1(sizeof(addr_node_t), addr_node);
}

//------------------------------------------------
Mcast::Mcast()
{
	next_syn_addr_tm  = 0x7FFFFFFF;
	next_del_addrs_tm = 0x7FFFFFFF;

	addr_mcast_fd = -1;
	mcast_fd = -1;
}

Mcast::~Mcast()
{

}

int 
Mcast::create_addr_mcast_socket()
{
	srand(time(0));

	next_del_addrs_tm = get_now_tv()->tv_sec + 100;
	svr_tbl = g_hash_table_new_full(g_str_hash, g_str_equal, 0, free_addr_cache);

	addr_mcast_fd = create_mcast_socket(config_get_strval("addr_mcast_ip"),
										config_get_strval("addr_mcast_port"),
										config_get_strval("addr_mcast_incoming_if"),
										mcast_rdwr, &addr_mcast_addr, &addr_mcast_len);
	if (addr_mcast_fd == -1) {
		ERROR_LOG("Failed to Create `addr_mcast_fd`: err=%d %s", errno, strerror(errno));
		return -1;
	}

	if (!is_parent) {
		mcast_pkg_header_t* hdr = (mcast_pkg_header_t*)addr_buf;
		addr_mcast_pkg_t* pkg = (addr_mcast_pkg_t*)hdr->body;
	
		hdr->proto_type = mcast_notify_addr;
		pkg->svr_id = get_server_id();
		strcpy(pkg->name, get_server_name());
		strcpy(pkg->ip, get_server_ip());
		pkg->port = get_server_port();
	}

	return g_sock_conn.add_one_conn(addr_mcast_fd, em_fd_type_addr_mcast, (sockaddr_in*)&addr_mcast_addr, 0);
}

void 
Mcast::syn_addr_info()
{
	time_t now_sec = get_now_tv()->tv_sec;
	// syn addr
	if (now_sec > next_syn_addr_tm) {
		send_addr_mcast_pkg(addr_mcast_syn_pkg);
	}
	// del expired addrs
	if (now_sec > next_del_addrs_tm) {
		del_expired_addrs();
	}
}

int 
Mcast::asynsvr_create_mcast_socket()
{
	mcast_fd = create_mcast_socket(config_get_strval("mcast_ip"),
									config_get_strval("mcast_port"),
									config_get_strval("mcast_incoming_if"),
									mcast_rdwr, &mcast_addr, &mcast_len);
	if (mcast_fd == -1) {
		ERROR_LOG("Failed to Create `mcast_fd`: err=%d %s", errno, strerror(errno));
		return -1;
	}

	return g_sock_conn.add_one_conn(mcast_fd, em_fd_type_mcast, (sockaddr_in*)&mcast_addr, 0);
}

void 
Mcast::send_addr_mcast_pkg(uint32_t pkg_type)
{
	mcast_pkg_header_t* hdr = (mcast_pkg_header_t*)addr_buf;

	hdr->pkg_type = pkg_type;
	sendto(addr_mcast_fd, addr_buf, sizeof(addr_buf), 0, (sockaddr*)&addr_mcast_addr, addr_mcast_len);
	next_syn_addr_tm  = time(0) + ranged_random(25, 48);
}

int 
Mcast::send_mcast_pkg(const void* data, int len)
{
	return sendto(mcast_fd, data, len, 0, (sockaddr*)&mcast_addr, mcast_len);
}

void 
Mcast::asyncserv_proc_mcast_pkg(void* data, int len)
{
	if (len < (int)sizeof(mcast_pkg_header_t)) {
		ERROR_LOG("invalid pkg len: %d", len);
		return;
	}

	mcast_pkg_header_t* pkg = (mcast_pkg_header_t*)data;
	switch (pkg->proto_type) {
		case mcast_notify_addr:
			if (!is_parent){
				proc_addr_mcast_pkg((mcast_pkg_header_t*)data, len);
			}
			break;
		case mcast_reload_text:
			proc_reload_plugin((reload_text_pkg_t*)pkg->body, len - sizeof(mcast_pkg_header_t));
			break;
		default:
			break;
	}
}

addr_node_t* 
Mcast::get_service_ipport(const char* service, unsigned int svr_id)
{
	addr_cache_t* ac = (addr_cache_t*)g_hash_table_lookup(svr_tbl, service);
	if (ac && g_hash_table_size(ac->addr_tbl)) {
		if (svr_id) {
			return (addr_node_t*)g_hash_table_lookup(ac->addr_tbl, &svr_id);
		} else {
			int n = rand() % g_hash_table_size(ac->addr_tbl);
			return (addr_node_t*)g_hash_table_find(ac->addr_tbl, addr_pred, &n);
		}
	}

	return 0;
}

void 
Mcast::del_expired_addrs()
{
	g_hash_table_foreach(svr_tbl, do_del_expired_addrs, 0);
	next_del_addrs_tm = get_now_tv()->tv_sec + 100;
}

//--------------------------------------------------------

void 
Mcast::proc_addr_mcast_pkg(const mcast_pkg_header_t* hdr, int len)
{
	if (len != (sizeof(addr_mcast_pkg_t) + sizeof(mcast_pkg_header_t))) {
		ERROR_LOG("invalid pkg len: %d, expected len: %lu",
					len, (unsigned long)(sizeof(addr_mcast_pkg_t) + sizeof(mcast_pkg_header_t)));
		return;
	}

	const addr_mcast_pkg_t* pkg = (addr_mcast_pkg_t*)hdr->body;
	// the same service
	if ( (strcmp(pkg->name, get_server_name()) == 0) 
			&& (pkg->svr_id == get_server_id()) ) {
		return;
	}

	/*if (dll.sync_service_info) {
		dll.sync_service_info(pkg->svr_id, pkg->name, pkg->ip, pkg->port, 1);
	}*/

	int new_node = 0;
	addr_cache_t* ac = (addr_cache_t*)g_hash_table_lookup(svr_tbl, pkg->name);
	if (!ac) {
		ac = (addr_cache_t*)g_slice_alloc(sizeof(*ac));
		strcpy(ac->svr_name, pkg->name);
		ac->addr_tbl = g_hash_table_new_full(g_int_hash, g_int_equal, 0, free_addr_node);
		g_hash_table_insert(svr_tbl, ac->svr_name, ac);
	}

	addr_node_t* addr_node = (addr_node_t*)g_hash_table_lookup(ac->addr_tbl, &(pkg->svr_id));
	if (!addr_node) {
		addr_node = (addr_node_t*)g_slice_alloc(sizeof(*addr_node));
		addr_node->svr_id = pkg->svr_id;
		g_hash_table_insert(ac->addr_tbl, &(addr_node->svr_id), addr_node);
		new_node = 1;

		INFO_LOG("ADD AN ADDR\t[name=%s id=%u ip=%s port=%d]",
					pkg->name, pkg->svr_id, pkg->ip, pkg->port);
	}
	if ( !new_node
			&& ((strncmp(addr_node->ip, pkg->ip, sizeof(addr_node->ip)) != 0)
			|| (addr_node->port != pkg->port))) {
		char buf[100];
		snprintf(buf, sizeof(buf), "%s.%s", pkg->name, "conflict");
		//asynsvr_send_warning(buf, pkg->svr_id, pkg->ip);

		EMERG_LOG("PROBABLY A SERVICE NAME CONFLICT\[name=%s id=%u %s:%u %s:%u]",
					pkg->name, pkg->svr_id, pkg->ip, pkg->port,
					addr_node->ip, addr_node->port);
	}
	strcpy(addr_node->ip, pkg->ip);
	addr_node->port        = pkg->port;
	addr_node->last_syn_tm = get_now_tv()->tv_sec;

	if (hdr->pkg_type == addr_mcast_1st_pkg) {
		send_addr_mcast_pkg(addr_mcast_syn_pkg);
	}
}

void 
Mcast::proc_reload_plugin(reload_text_pkg_t* pkg, int len)
{
	if (len != sizeof(reload_text_pkg_t)) {
		ERROR_LOG("invalid len: %d, expected len: %lu",
					len, (unsigned long)sizeof(reload_text_pkg_t));
		return;
	}

	if (!is_parent) {
		if ( strcmp(pkg->svr_name, get_server_name()) 
				|| (pkg->svr_id && (pkg->svr_id != get_server_id())) ) {
			return;
		}
	} else {
		//bind_config_t* bc = get_bind_conf();
		if ( (g_binds.bind_num == 0)
				|| strcmp(pkg->svr_name, g_binds.configs[0].server_name)
				|| (pkg->svr_id != 0) ) {
			return;
		}
	}

	pkg->new_so_name[sizeof(pkg->new_so_name) - 1] = '\0';
	/*if (dll.before_reload && (dll.before_reload(is_parent) == -1)) {
		exit(-1);
	}*/

	g_dll.unregister_plugin();

	DEBUG_LOG("RELOAD %s", pkg->new_so_name);

	if (g_dll.register_plugin(pkg->new_so_name, 1) == -1) {
		exit(-1);
	}

	/*if (!is_parent && dll.reload_global_data && (dll.reload_global_data() == -1)) {
		exit(-1);
	}*/
}

