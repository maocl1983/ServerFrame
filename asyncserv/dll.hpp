/**
 *============================================================
 *  @file        dll.h
 *  @brief      Define the interfaces that a .so must implement to make use of AsyncServ
 * 
 *  compiler   gcc4.1.2
 *  platform   Linux
 *
 *============================================================
 */

#ifndef ASYNC_SERVER_DLL_H_
#define ASYNC_SERVER_DLL_H_

#include <sys/types.h>
#include "worker.hpp"
/**
 * @struct AsyncServInterface
 * @brief To make use of AsyncServ, a .so must implement the interfaces held by this structure
 *
 */
class DllInterface {
public:
	/* The following 5 interfaces are called only by the child process */
	int (*proc_pkg_from_client)(void* pkg, int pkglen, fdsession_t* fdsess);
	void (*proc_pkg_from_serv)(int fd, void* pkg, int pkglen);
	void (*proc_mcast_pkg)(const void* data, int len);
	void (*on_client_conn_closed)(int fd);
	void (*on_fd_closed)(int fd);
	void (*proc_events)();

	/* The following 3 interfaces are called both by the parent and child process */
	int	(*init_service)(int isparent);
	int (*fini_service)(int isparent);
	int (*get_pkg_len)(int fd, const void* avail_data, int avail_len, int isparent);
	int (*proc_udp_pkg)(int fd, const void* avail_data, int avail_len ,struct sockaddr_in * from, socklen_t fromlen );
public:
	DllInterface();
	int register_plugin(const char* file_name, int flag);
	void unregister_plugin();
private:
	void*   handle; // Hold the handle returned by dlopen
};

extern DllInterface g_dll;

#endif // ASYNC_SERVER_DLL_H_
