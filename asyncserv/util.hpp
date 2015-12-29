#ifndef ASYNC_SERVER_UTIL_H_
#define ASYNC_SERVER_UTIL_H_

extern "C" {
#include <libcommon/log.h>
#include <libcommon/conf_parser/config.h>
}

#ifdef  likely
#undef  likely
#endif
#define likely(x)  __builtin_expect(!!(x), 1)

#ifdef  unlikely
#undef  unlikely
#endif
#define unlikely(x)  __builtin_expect(!!(x), 0)

static inline int init_logfile(int server_id = 0)
{
	char prefix[10] = {0};
	if (server_id > 0) {//子进程
		int len = snprintf(prefix, 8, "%u", server_id);
		prefix[len] = '_';
	}

	const char* log_dir = config_get_strval("log_dir");
	log_lvl_t log_lv = (log_lvl_t)config_get_intval("log_level", log_lvl_trace);
    uint32_t log_size = config_get_intval("log_size", 1<<30);
	uint32_t maxfiles = config_get_intval("max_log_files", 100);
	const char* log_pre = prefix;	
	int ret = log_init(log_dir, log_lv, log_size, maxfiles, log_pre);

	BOOT_LOG(ret, "set log dir %s, per file size %gMB", log_dir, log_size / 1024.0 / 1024.0);
}


#endif // ASYNC_SERVER_UTIL_H_
