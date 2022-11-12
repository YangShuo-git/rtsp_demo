﻿#ifndef _rtp_util_h_
#define _rtp_util_h_

#include "rtp-header.h"

// 网络协议定义大端序为标准网络字节序 nbo是网络字节序
#define nbo_r16 rtp_read_uint16
#define nbo_r32 rtp_read_uint32
#define nbo_w16 rtp_write_uint16
#define nbo_w32 rtp_write_uint32

static inline uint16_t rtp_read_uint16(const uint8_t* ptr)
{
    return (((uint16_t)ptr[0]) << 8) | ptr[1];
}

static inline uint32_t rtp_read_uint32(const uint8_t* ptr)
{
    return (((uint32_t)ptr[0]) << 24) | (((uint32_t)ptr[1]) << 16) | (((uint32_t)ptr[2]) << 8) | ptr[3];
}

// 如何将val写入ptr数组中
static inline void rtp_write_uint16(uint8_t* ptr, uint16_t val)
{
    ptr[0] = (uint8_t)(val >> 8);
    ptr[1] = (uint8_t)val;
}

static inline void rtp_write_uint32(uint8_t* ptr, uint32_t val)
{
    ptr[0] = (uint8_t)(val >> 24);
    ptr[1] = (uint8_t)(val >> 16);
    ptr[2] = (uint8_t)(val >> 8); 
    ptr[3] = (uint8_t)val;
}

static inline void nbo_write_rtp_header(uint8_t *ptr, const rtp_header_t *header)
{
    ptr[0] = (uint8_t)((header->v << 6) | (header->p << 5) | (header->x << 4) | header->cc); //这是需要把8位整合到一起，再赋值
    ptr[1] = (uint8_t)((header->m << 7) | header->pt);
    ptr[2] = (uint8_t)(header->seq >> 8);
    ptr[3] = (uint8_t)(header->seq & 0xFF);

    nbo_w32(ptr+4, header->timestamp);  //这是正常的赋值
    nbo_w32(ptr+8, header->ssrc);
}


#endif /* !_rtp_util_h_ */
