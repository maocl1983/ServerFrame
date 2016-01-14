#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <signal.h>

extern "C" {
#include <libcommon/log.h>
#include <libcommon/conf_parser/config.h>
#include <libcommon/tm_dirty/tm_dirty.h>
}

#include "binding.hpp"
#include "daemon.hpp"
#include "dll.hpp"
#include "socket.hpp"
#include "worker.hpp"
#include "util.hpp"
#include "mcast.hpp"

#define DEF_PAGE_SIZE 8192
static const char version[] = "1.6.3";

void show_banner()
{
	char feature[256];
	int pos = 0;
	
#ifdef DEBUG	
	pos = sprintf (feature + pos, "-DDEBUG -g ");
#endif
	pos = sprintf (feature + pos, "-DEPOLL_MODE ");

	INFO_LOG("Async Server v%s", version);
	INFO_LOG("Compiled at %s %s, flag: %s\n", __DATE__, __TIME__, pos ? feature : "");
}

static inline void
show_usage()
{
	INFO_LOG("Usage: %s conf\n", g_daemon.prog_name);
	exit(-1);
}

static inline void
parse_args(int argc, char** argv)
{
	g_daemon.prog_name = strdup(argv[0]);
	g_daemon.current_dir = get_current_dir_name();
	show_banner();
	if ( (argc < 2) || !strcmp(argv[1], "--help") || !strcmp(argv[1], "-h") ) {
		show_usage();
	}
}

int main(int argc, char* argv[])
{
	parse_args(argc, argv);
	char *p_conf_file=argv[1];

	if (config_init(p_conf_file ) == -1) {
		BOOT_LOG(-1, "Failed to Parse File '%s'", argv[1]);
	}

	g_daemon.start(argc, argv);
	// load bind config file
	g_binds.load_bind_file(config_get_strval("bind_conf"));

	// init log files
	init_logfile();
	socket_timeout = config_get_intval("cli_socket_timeout", 0);
	page_size      = config_get_intval("incoming_packet_max_size", -1);
	g_send_buf_limit_size = config_get_intval("send_buf_limit_size", 0);
	if (page_size <= 0) {
		page_size = DEF_PAGE_SIZE;
	}

    //asynsvr_init_warning_system();

	g_dll.register_data_plugin(config_get_strval("data_dll_file"));
	g_dll.register_plugin(config_get_strval("dll_file"), 0);

	g_sock_conn.init(g_daemon.max_fd_num, g_daemon.max_fd_num);
	if (g_dll.init_service && (g_dll.init_service(1) != 0)) {
		BOOT_LOG(-1, "FAILED TO INIT PARENT PROCESS");
	}

    g_daemon.clean_child_pids();

	int   i;
	pid_t pid;
	for ( i = 0; i != g_binds.bind_num; ++i ) {
		BindElem* bc_elem = &(g_binds.configs[i]);
		bc_elem->init_shmq();

		if ( (pid = fork ()) < 0 ) {
			BOOT_LOG(-1, "fork child process");
		} else if (pid > 0) { //parent process
			bc_elem->close_shmq(em_parent_type);
			ERROR_LOG("main:do add conn[%d]",bc_elem->sendq.get_pipe_handle(0));
			bc_elem->add_pipe_conn(em_send_pipe);
			bc_elem->add_listen_conn();
			g_daemon.set_child_pid(i,pid);
		} else { //child process
			//g_listen_port = bc_elem->bind_port;
			//strncpy(g_listen_ip, bc_elem->bind_ip, sizeof(g_listen_ip) - 1);
			g_work_svr.run_child_process(i, i + 1);
		}
	}

	if (config_get_strval("addr_mcast_ip")) {
		if (g_mcast.create_addr_mcast_socket() != 0) {
			BOOT_LOG(-1, "PARENT: FAILED TO CREATE MCAST FOR RELOADING SO");
		}
	} 
    static int stop_count = 0;
	while (1) {
        if (unlikely(g_daemon.stop_flag == 1 && g_daemon.term_signal == 1 && stop_count++ == 0))
            DEBUG_LOG("SIG_TERM from pid=%d", getpid());
        if (unlikely(g_daemon.stop_flag == 1 && g_dll.fini_service && (g_dll.fini_service(1) == 0)))
            break;

        g_sock_conn.net_loop(-1, page_size, 1);
	}

    g_daemon.killall_children();

	g_sock_conn.exit();
	g_dll.unregister_plugin();
	g_binds.destroy_all_shmq();
	g_daemon.stop();

	return 0;
}
