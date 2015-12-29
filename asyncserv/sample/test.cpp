
extern "C" {
#include <libcommon/list.h>
#include <libcommon/log.h>
}

#include "test.hpp"

/*typedef struct TmrTest {
	list_head_t timer_list;

} tmr_test_t;

tmr_test_t tmr;
*/
/*int test(void* owner, void* data)
{
	send_pkg_to_client(data, "111v", 5);
	return 0;
}*/

#pragma pack(1)
struct cli_proto_head_t {
	uint32_t len; /* 协议的长度 */
	uint32_t cmd; /* 协议的命令号 */
	uint32_t user_id; /* 用户的米米号 */
	uint32_t seq_num;/* 序列号 */
	uint32_t ret; /* S->C, 错误码 */
	uint8_t body[]; /* 包体信息 */
};
#pragma pack()

extern "C" int init_service(int isparent)
{
	//DEBUG_LOG("INIT...:%s:%d",g_listen_ip,g_listen_port );
	DEBUG_LOG("INIT...");
	if (!isparent) {
		//setup_timer();
		//INIT_LIST_HEAD(&tmr.timer_list);
	}
	return 0;
}

extern "C" int fini_service(int isparent)
{
	DEBUG_LOG("FINI...");
	return 0;
}

extern "C" void proc_events()
{
	//handle_timer();
}

extern "C" int	get_pkg_len(int fd, const void* pkg, int pkglen, int isparent)
{
	int len = -1;
	if (isparent) {
		/*int i;

		const char* str = (char*)pkg;
		for (i = 0; (i != pkglen) && (str[i] != '\0'); ++i) ;

		if (i != pkglen) {
			net_send(fd, pkg, i + 1);
			return i + 1;
		}*/
		cli_proto_head_t* header = (cli_proto_head_t*)pkg;
		len = header->len;
	} else {
		len = *(uint32_t*)(pkg);
	}

	DEBUG_LOG("get pkg len=[len=%d fd=%d]",len,fd);
	return len;
}

extern "C" int proc_pkg_from_client(void* data, int len, fdsession_t* fdsess)
{
	cli_proto_head_t* header = (cli_proto_head_t*)data;
	DEBUG_LOG("pkgs len=[%d]",len);
	DEBUG_LOG("%.*s", len-sizeof(cli_proto_head_t), (char*)header->body);
	//send_pkg_to_client(fdsess, data, len);
	//net_send(fdsess->fd, data, len);
	send_pkg_to_client(fdsess, data, len);
	//ADD_TIMER_EVENT(&tmr, test, fdsess, get_now_tv()->tv_sec + 5);
	//return 0;
	return 0;
}

extern "C" void proc_pkg_from_serv(int fd, void* data, int len)
{
}

extern "C" void on_client_conn_closed(int fd)
{
}

extern "C" void on_fd_closed(int fd)
{
}

