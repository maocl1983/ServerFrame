#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <fcntl.h>

extern "C" {
#include <libcommon/log.h>
#include <libcommon/conf_parser/config.h>
}
#include "binding.hpp"
#include "shmq.hpp"
#include "daemon.hpp"
#include "util.hpp"
#include "net_if.hpp"

ShmqQueue::ShmqQueue()
{

}

ShmqQueue::~ShmqQueue()
{
	shmq_addr = 0;
	length = 0;
}

int 
ShmqQueue::get_pipe_handle(int i)
{
	return pipe_handles[i];
}

int
ShmqQueue::create()
{
	length = config_get_intval("shmq_length", 1 << 26);
	shmq_addr = (shmq_head_t*) mmap (NULL, length, PROT_READ | PROT_WRITE, 
			MAP_SHARED | MAP_ANON, -1, 0);
	if (shmq_addr == MAP_FAILED)
		ERROR_RETURN (("mmap failed, %s", strerror (errno)), -1);

	shmq_addr->head = sizeof(shmq_head_t);
	shmq_addr->tail = sizeof(shmq_head_t);
	atomic_set(&(shmq_addr->block_cnt), 0);
	int err = create_pipe();
	//DEBUG_LOG("pipe create[%d %d]",pipe_handles[0], pipe_handles[1]);
	BOOT_LOG (err, "create shmq memory queue: %dMB", length / 1024 / 512);
	return 0;
}

void
ShmqQueue::destroy()
{
	if (shmq_addr) {
		munmap(shmq_addr, length);
		shmq_addr = 0;
	}

}

void
ShmqQueue::close_pipe(int i)
{
	//DEBUG_LOG("close pipe[%d][%d %d]",i, pipe_handles[0], pipe_handles[1]);
	if (i == 2) {
		close(pipe_handles[0]);
		close(pipe_handles[1]);
	} else {
		close(pipe_handles[i]);
	}
}

int
ShmqQueue::pop_data(struct shmq_block_t** blk)
{
	//queue is empty
	if (shmq_addr->tail == shmq_addr->head) {
		return -1;
	}

	set_tail_pos();
	//queue is empty
	if (shmq_addr->tail == shmq_addr->head) {
		return -1;
	}

	shmq_block_t* cur_blk = tail_block();
	int head_pos = shmq_addr->head;
#ifdef DEBUG
	//tail block overflow
	if (cur_blk->len < sizeof(struct shmq_block_t)
		|| (shmq_addr->tail < head_pos && shmq_addr->tail + (int)cur_blk->len > head_pos)
		|| (shmq_addr->tail > head_pos && shmq_addr->tail + (int)cur_blk->len > (int)length))	{
		ERROR_LOG("shm_queue tail=%d,head=%d,shmq_block length=%d", shmq_addr->tail, head_pos, cur_blk->len);
		exit(-1);
	}
#endif
	if (cur_blk->len > (uint32_t)page_size)
		ERROR_RETURN(("too large block, len=%d %d", cur_blk->len, page_size), -1);
	
	*blk = cur_blk;
	shmq_addr->tail += cur_blk->len;

	//TRACE_LOG("pop queue: q=%p length=%d tail=%d head=%d id=%u count=%u fd=%d",
	//			shmq_addr, cur_blk->len, shmq_addr->tail, shmq_addr->head, (*blk)->id,
	//			(atomic_dec(&shmq_addr->block_cnt), atomic_read(&shmq_addr->block_cnt)), cur_blk->fd);
	return 0;
}

int 
ShmqQueue::push_data(shmq_block_t* blk, const void* data)
{
	assert(blk->len >= sizeof(shmq_block_t));

	if (blk->len > (uint32_t)page_size) {
		ERROR_LOG("too large packet, len=%d", blk->len);
		return -1;
	}

	if (set_head_pos(blk) == -1) {
		return -1;
	}

	int wait_tm;
	for (wait_tm = 0; wait_tm != 10; ++wait_tm) {
		//queue is full, (page_size): prevent overwriting the buffer which shmq_pop refers to
		if (unlikely( (shmq_addr->tail > shmq_addr->head) && 
					  (shmq_addr->tail < shmq_addr->head + (int)blk->len + page_size)) ) {
			ALERT_LOG("queue [%p] is full, wait 5 microsecs: [wait_tm=%d]", this, wait_tm);
			usleep(5);
		} else {
			break;
		}
	}

	if (unlikely(wait_tm == 10)) {
		ALERT_LOG("queue [%p] is full.", this);
		return -1;
	}

	char* next_blk = (char*)head_block();
	memcpy(next_blk, blk, sizeof(shmq_block_t));
	if (likely(blk->len > sizeof(shmq_block_t))) {
		memcpy(next_blk + sizeof(shmq_block_t), data, blk->len - sizeof(shmq_block_t));
	}

	shmq_addr->head += blk->len;

	//通知
	write(pipe_handles[1], this, 1);
	//TRACE_LOG("push queue: queue=%p,length=%d,tail=%d,head=%d,id=%u,count=%d,fd=%d",
	//			 shmq_addr, blk->len, shmq_addr->tail, shmq_addr->head, blk->id,
	//			 (atomic_inc(&shmq_addr->block_cnt), atomic_read(&shmq_addr->block_cnt)), blk->fd);
	return 0;
}

int 
ShmqQueue::create_pipe()
{
	if (pipe(pipe_handles) == -1)
		return -1;

    int rflag, wflag;
    if (config_get_intval("set_pipe_noatime", 0) == 1) {
        rflag = O_NONBLOCK | O_RDONLY | O_NOATIME;
        wflag = O_NONBLOCK | O_WRONLY | O_NOATIME;
    } else {
        rflag = O_NONBLOCK | O_RDONLY;
        wflag = O_NONBLOCK | O_WRONLY;
    }

	fcntl(pipe_handles[0], F_SETFL, rflag);
	fcntl(pipe_handles[1], F_SETFL, wflag);

	fcntl(pipe_handles[0], F_SETFD, FD_CLOEXEC);
	fcntl(pipe_handles[1], F_SETFD, FD_CLOEXEC);

	return 0;
}

int
ShmqQueue::set_tail_pos()
{
	struct shmq_block_t *blk;
	if (likely(shmq_addr->head >= shmq_addr->tail)) {
		return 0;
	}

	blk = tail_block();
	if (length - shmq_addr->tail < sizeof (shmq_block_t) 
			|| blk->type == em_dust_block) {
		shmq_addr->tail = sizeof(shmq_head_t);
	}

	return 0;
}

int
ShmqQueue::set_head_pos(const struct shmq_block_t *blk)
{
	int tail_pos = shmq_addr->tail;
	int head_pos = shmq_addr->head;

	int surplus = length - head_pos;

	if (unlikely(surplus < (int)blk->len))
	{
		if (unlikely(tail_pos > head_pos)) {
			ERROR_LOG("shm_queue bug, head=%d, tail=%d, blk_len=%d, total_len=%u",
						head_pos, tail_pos, blk->len, length);
			shmq_addr->tail = sizeof(shmq_head_t);
			shmq_addr->head = sizeof(shmq_head_t);
		} else if (unlikely(tail_pos == sizeof (shmq_head_t))){
			//尾部位置还在队列开头，表明queue已满
			ERROR_RETURN (("shm_queue is full,head=%d,tail=%d,blk_len=%d",
				head_pos, tail_pos, blk->len), -1);
		} else if (unlikely(surplus < (int)sizeof(shmq_block_t))) {
			shmq_addr->head = sizeof (shmq_head_t);
		} else {
			//剩余的标志为垃圾块
			struct shmq_block_t *dust_blk;
			dust_blk = head_block();
			dust_blk->type = em_dust_block;
			dust_blk->len = surplus;
			dust_blk->id = 0;
			shmq_addr->head = sizeof(shmq_head_t);
		}
	}

	return 0;
}
