#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <linux/unistd.h>

extern "C" {
#include <libcommon/log.h>
#include <libcommon/conf_parser/config.h>
}
#include "shmq.hpp"
#include "daemon.hpp"


Daemon g_daemon;

static void sigterm_handler(int signo) 
{
	DEBUG_LOG("sigterm_handler[%d]",signo);
	g_daemon.stop_flag     = 1;
	g_daemon.restart_flag  = 0;
    g_daemon.term_signal = 1;
}

static void sighup_handler(int signo) 
{
	DEBUG_LOG("sighup_handler[%d]",signo);
	g_daemon.restart_flag  = 1;
	g_daemon.stop_flag     = 1;
}

static void sigchld_handler(int signo, siginfo_t *si, void * p) 
{
	DEBUG_LOG("sigchld_handler[%d]",signo);
    pid_t pid;
	while ((pid = waitpid (-1, &g_daemon.status, WNOHANG)) > 0) {
        int i;
        for (i = 0; i < g_binds.bind_num; ++i) {
            if (g_daemon.get_child_pid(i) == pid) {
				g_daemon.set_child_pid(i,0);
				break;
            }
        }
	}
}

Daemon::Daemon()
{
	stop_flag = 0;
	restart_flag = 0;
	term_signal = 0;

	saved_argv = NULL;
	backgd_mode = 0;
}

Daemon::~Daemon()
{

}

void 
Daemon::set_proc_title(const char* fmt, ...)
{
	char title[128];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(title, sizeof(title) - 1, fmt, ap);
	va_end(ap);

	int title_len = strlen(title) + 1;
	if (argv_end - argv_begin < title_len && env_begin == argv_end) {
		char *env_end = env_begin;
		for (int i = 0; environ[i]; i++) {
			if(env_end == environ[i]) {
				env_end = environ[i] + strlen (environ[i]) + 1;
				environ[i] = strdup(environ[i]);
			} else {
				break;
			}
		}
		argv_end = env_end;
		env_begin = NULL;
	}

	int argv_len = argv_end - argv_begin;
	if (title_len == argv_len) {
		strcpy (argv_begin, title);
	} else if (title_len < argv_len) {
		strcpy (argv_begin, title);
		memset (argv_begin + title_len, 0, argv_len - title_len);
	} else {
		stpncpy(argv_begin, title, argv_len - 1)[0] = '\0';
	}
}

void
Daemon::set_rlimit()
{
	struct rlimit rlim;
	max_fd_num = config_get_intval("max_open_fd", 20000);

	/* 最大文件数的设置 */
	rlim.rlim_cur = max_fd_num;
	rlim.rlim_max = max_fd_num;
	if (setrlimit(RLIMIT_NOFILE, &rlim) == -1) {
		ALERT_LOG("INIT FD RESOURCE FAILED");
	}

	/* core文件的设置 */
	rlim.rlim_cur = 1 << 30;
	rlim.rlim_max = 1 << 30;
	if (setrlimit(RLIMIT_CORE, &rlim) == -1) {
		ALERT_LOG("INIT CORE FILE RESOURCE FAILED");
	}
}

void
Daemon::fini_free()
{
	char** argv;
	for ( argv = saved_argv; *argv; ++argv ) {
		free(*argv);
	}
	free(saved_argv);
	saved_argv = NULL;
	
	free(prog_name);
	free(current_dir);
}

void
Daemon::dup_argv(int argc, char** argv)
{
	argv_begin = argv[0];
	argv_end = argv[argc-1] + strlen(argv[argc - 1]) + 1;
	env_begin = environ[0];

	saved_argv = (char**)malloc(sizeof(char*) * (argc + 1));
	if (!saved_argv) {
		return;
	}
	saved_argv[argc] = NULL;
	while (--argc >= 0) {
		saved_argv[argc] = strdup(argv[argc]);
	}
}

void
Daemon::init_signal()
{
	struct sigaction sa;
	sigset_t sset;

	memset(&sa, 0, sizeof(sa));
	signal(SIGPIPE,SIG_IGN);	

	sa.sa_handler = sighup_handler;
	sigaction(SIGHUP, &sa, NULL);

	sa.sa_handler = sigterm_handler;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);
	
	sa.sa_flags = SA_RESTART|SA_SIGINFO;
	sa.sa_sigaction = sigchld_handler;
	sigaction(SIGCHLD, &sa, NULL);
	
	sigemptyset(&sset);
	sigaddset(&sset, SIGABRT);
	sigaddset(&sset, SIGILL);
	sigaddset(&sset, SIGCHLD);
	sigaddset(&sset, SIGSEGV);
	sigaddset(&sset, SIGBUS);
	sigaddset(&sset, SIGFPE);
	sigprocmask(SIG_UNBLOCK, &sset, &sset);
}

int 
Daemon::start(int argc, char** argv)
{
	set_rlimit();
	init_signal();
	dup_argv(argc, argv);

	const char* style = config_get_strval ("run_mode");
	if (!style || !strcasecmp ("background", style)) {
		daemon(1, 1);
		backgd_mode = 1;
		BOOT_LOG (0, "switch to daemon mode");
	}
	return 0;
}

void 
Daemon::stop(void) 
{
	if (!backgd_mode) {
		printf("Server stopping...\n");
	}

	if (restart_flag && prog_name && saved_argv) {
		WARN_LOG("%s", "Server restarting...");
		chdir(current_dir);
		execv(prog_name, saved_argv);
		WARN_LOG("%s", "Restart Failed...");
	}

	fini_free();
}

void 
Daemon::clean_child_pids()
{
    int i;
    for (i = 0; i < max_listen_fds; ++i) {
        atomic_set(&child_pids[i], 0);
    }
}

void 
Daemon::killall_children()
{
    int i;
    for (i = 0; i < g_binds.bind_num; ++i) {
        if (atomic_read(&child_pids[i]) != 0) {
            kill(atomic_read(&child_pids[i]), SIGTERM);
        }
    }

    /* wait for all child exit*/
    for (int i = 0; i < g_binds.bind_num; ++i) {
    	if (atomic_read(&child_pids[i]) != 0) {
            usleep(100);
            i = 0;
        }
    }
}

void
Daemon::set_child_pid(int idx, int pid)
{
	atomic_set(&child_pids[idx], pid);
}

int
Daemon::get_child_pid(int idx)
{
	return atomic_read(&child_pids[idx]);
}
