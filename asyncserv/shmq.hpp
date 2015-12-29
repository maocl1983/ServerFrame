#ifndef ASYNC_SERVER__SHMQ_H_
#define ASYNC_SERVER__SHMQ_H_

#include <stdint.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <unistd.h>

#include <libcommon/atomic.h>

#include "socket.hpp"

#define LOCKED_MASK	0x01
#define SLEEP_MASK	0x02
#define SHM_BLOCK_LEN_MAX	1<<23

/*enum shmq_block_type_t{
	DATA_BLOCK = 0,
	PAD_BLOCK,
	FIN_BLOCK,	// child informs parent to close a connection
	OPEN_BLOCK,
	CLOSE_BLOCK	// parent informs child that a connection had been closed
};*/

enum shmq_block_type_t{
	em_data_block = 0,
	em_dust_block,
	em_fin_block,	// child informs parent to close a connection
	em_open_block,
	em_close_block,	// parent informs child that a connection had been closed
};

struct shmq_head_t {
	volatile int head;
	volatile int tail;
	atomic_t block_cnt;
} __attribute__ ((packed));

struct shmq_block_t {
	uint32_t	id;
	uint32_t	fd;
	uint32_t	len;	// length of the whole shmblock, including data
	char		type;
	uint8_t		data[];
} __attribute__ ((packed));

class ShmqQueue {
public:
	ShmqQueue();
	~ShmqQueue();

	int create();
	void destroy();

	int get_pipe_handle(int i);

	int pop_data(struct shmq_block_t** blk);
	int push_data(shmq_block_t* mb, const void* data);
	void close_pipe(int i = 2);
private:
	inline struct shmq_block_t* head_block()
	{return (struct shmq_block_t*)((char *)shmq_addr + shmq_addr->head);}
	inline struct shmq_block_t* tail_block()
	{return (struct shmq_block_t*)((char *)shmq_addr + shmq_addr->tail);}
	
	int create_pipe();
	int set_tail_pos();
	int set_head_pos(const struct shmq_block_t *blk);
private:
	shmq_head_t* shmq_addr;
	uint32_t length;
	int pipe_handles[2];
};




#endif // ASYNC_SERVER__SHMQ_H_
