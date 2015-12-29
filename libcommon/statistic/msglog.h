/**
 * =====================================================================================
 *       @file  msglog.h
 *      @brief  用于写入统计信息
 *
 *    Revision  3.0.0
 *    Compiler  gcc/g++
 * =====================================================================================
 */

#ifndef COMMON_STATISTIC_H_
#define COMMON_STATISTIC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#pragma pack(1)
struct stat_msg_header_t {
	uint16_t len;
	uint16_t sid;
	uint32_t msg_id;
	uint32_t timestamp;
	uint8_t from_type;
	uint8_t	data[];
};
#pragma pack()

/** 
 * @brief  写入统计信息
 * @param   logfile 统计信息的写入路径
 * @param   统计信息分类号，即msgid
 * @param   timestamp 统计信息产生时间，可用time(0)产生
 * @param   data 具体的统计信息，以4个字节4个字节的形式储存
 * @param   len data的长度，肯定是4的倍数
 * @return  0成功，-1失败
 */
int statistic_log(const char* logfile, uint32_t sid, uint32_t msgid, uint32_t timestamp, const void* data, int len); 

#ifdef __cplusplus
}
#endif

#endif /* COMMON_STATISTIC_H_ */

