#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

extern "C" {
#include <libcommon/log.h>
}
#include "dll.hpp"

DllInterface g_dll;

#define DLFUNC_NO_ERROR(h, v, name) \
		do { \
			v = (typeof(v))dlsym(h, name); \
			dlerror(); \
		} while (0)

#define DLFUNC(h, v, name) \
		do { \
			v = (typeof(v))dlsym(h, name); \
			if ((error = dlerror ()) != NULL) { \
				ERROR_LOG("dlsym error, %s", error); \
				dlclose(h); \
				h = NULL; \
				goto out; \
			} \
		} while (0)

DllInterface::DllInterface()
{
	handle = 0;
	init_service = 0;
	fini_service = 0;
	proc_events = 0;
	proc_mcast_pkg = 0;
	proc_udp_pkg = 0;

	get_pkg_len = 0;
	proc_pkg_from_client = 0;
	proc_pkg_from_serv = 0;
	on_client_conn_closed = 0;
	on_fd_closed = 0;
}

int 
DllInterface::register_plugin(const char* file_name, int flag)
{
	char* error; 
	int   ret_code = -1;
	
	handle = dlopen(file_name, RTLD_NOW);
	if ((error = dlerror()) != NULL) {
		ERROR_LOG("dlopen error, %s", error);
		goto out;
	}
	
	DLFUNC_NO_ERROR(handle, init_service, "init_service");
	DLFUNC_NO_ERROR(handle, fini_service, "fini_service");
	DLFUNC_NO_ERROR(handle, proc_events, "proc_events");
	DLFUNC_NO_ERROR(handle, proc_mcast_pkg, "proc_mcast_pkg");
	DLFUNC_NO_ERROR(handle, proc_udp_pkg, "proc_udp_pkg");

	DLFUNC(handle, get_pkg_len, "get_pkg_len");
	DLFUNC(handle, proc_pkg_from_client, "proc_pkg_from_client");
	DLFUNC(handle, proc_pkg_from_serv, "proc_pkg_from_serv");
	DLFUNC(handle, on_client_conn_closed, "on_client_conn_closed");
	DLFUNC(handle, on_fd_closed, "on_fd_closed");

	//DLFUNC_NO_ERROR(dll.handle, dll.before_reload, "before_reload");
	//DLFUNC_NO_ERROR(dll.handle, dll.reload_global_data, "reload_global_data");
	//DLFUNC_NO_ERROR(dll.handle, dll.sync_service_info, "sync_service_info");

	ret_code = 0;

out:
	if (!flag) {
		BOOT_LOG(ret_code, "dlopen %s", file_name);
	} else {
		DEBUG_LOG("RELOAD %s\t[%s]", file_name, (ret_code ? "FAIL" : "OK"));
		return ret_code;
	}
}

void 
DllInterface::unregister_plugin()
{
	if (handle != NULL){
		dlclose(handle);
		handle = NULL;
	}
}

