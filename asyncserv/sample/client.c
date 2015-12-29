#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#include <openssl/md5.h>

int connect_to_svr(const char* ipaddr, uint16_t port)
{
	struct sockaddr_in peer;
	int fd;

	bzero(&peer, sizeof (peer));
	peer.sin_family  = AF_INET;
	peer.sin_port    = htons(port);
	if (inet_pton (AF_INET, ipaddr, &peer.sin_addr) <= 0) {
        return -1;
    }
    
    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    connect(sockfd, (const struct sockaddr *)&peer, sizeof(peer));

	return sockfd;
}

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

void *send_data(void *arg)
{
	while (1) {
		pthread_detach(pthread_self());
		int sockfd = *(int *)arg;
    	char buf[8192] = {0};
		//const char *data = "Hello Frankie!";
		char data[4096] = {};
		int i = 0;
		for (; i < 4095; i++) {
			data[i] = 'a' + i % 26;
		}
		uint32_t body_len = strlen(data);
		struct cli_proto_head_t *header = (struct cli_proto_head_t*)malloc(20 + body_len);
		memset(header, 0, 20 + body_len);
		header->cmd = 101;
		header->user_id = 10001;
		header->len = 20 + body_len;
		memcpy(header->body, data, body_len);

		write(sockfd, header, header->len);
		//printf("send %d bytes", 20 + body_len);
		printf("Hello World ");
		/*
		ssize_t n = 0;
		while ((n = read(sockfd, buf, 4096)) > 0) {
			printf("read %d bytes data!\n", n);
			printf("%.*s\n", n, buf);
		}*/
		free(header);
		usleep(10000);
	}

	return NULL;
}

int main(int argc, char **argv)
{
    int sockfd = connect_to_svr("192.168.10.181", 8165);

	int i = 0;
	pthread_t tid[10000];
	//for (; i < 10000; i++) {
		pthread_create(&tid[0], 0, send_data, &sockfd);
	//}
	sleep(100);
	close(sockfd);
}
