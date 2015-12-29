#ifndef ASYNC_SERVER_DAEMON_H_
#define ASYNC_SERVER_DAEMON_H_

#include <stdlib.h>

extern "C" {
#include <libcommon/atomic.h>
}

#include "binding.hpp"


class Daemon {
public:
	Daemon();
	~Daemon();
	
	int start(int argc, char** argv);
	void stop(void);

	void set_proc_title(const char* fmt, ...);

	void clean_child_pids();
	void killall_children();
	void set_child_pid(int idx, int pid);
	int get_child_pid(int idx);
	
	void fini_free();
private:
	void init_signal();
	void set_rlimit();
	void dup_argv(int argc, char** argv);
public:
	int max_fd_num;

	volatile int stop_flag;
	volatile int restart_flag;
	volatile int term_signal;
	int	status;
	char* prog_name;
	char* current_dir;

private:
	char* argv_begin;
	char* argv_end;
	char* env_begin;

	char** saved_argv;
	int	backgd_mode;
	atomic_t child_pids[max_listen_fds];
};

extern Daemon g_daemon;

#endif // ASYNC_SERVER_DAEMON_H_
