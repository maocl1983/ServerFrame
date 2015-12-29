#include <assert.h>
#include <string.h>

#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/signal.h>
#include <sys/stat.h>

#include "msglog.h"

enum {
	buf_size	= 4096
};

int statistic_log(const char* logfile, uint32_t sid, uint32_t msgid, uint32_t timestamp, const void* data, int len)
{
	char buf[buf_size];
    struct stat_msg_header_t *head;
    int fd, total_len;

	total_len = sizeof(struct stat_msg_header_t)+len;

	assert((len >= 0) && (total_len >= (int)sizeof(struct stat_msg_header_t)) && (total_len <= buf_size));

	head = (void*)buf;

	memset(head, 0, sizeof(*head));

    head->len = total_len;
    head->sid = sid;
    head->msg_id = msgid;
    head->timestamp = timestamp;
	head->from_type = 1;

    if(len > 0) {
		memcpy((char *)(head->data), data, len);
	}	

    signal(SIGXFSZ, SIG_IGN);
    fd = open(logfile, O_WRONLY|O_APPEND, 0666);
    if(fd<0) 
    {
        fd = open(logfile, O_WRONLY|O_APPEND|O_CREAT, 0666);
        int ret=fchmod(fd,0777);
        if ((ret!=0)||(fd<0))
        {
            return -1;
        }
    }

    write(fd, (char *)head, total_len);
    close(fd);

    signal(SIGXFSZ, SIG_DFL);
    return 0;
}
