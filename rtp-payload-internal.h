﻿#ifndef _rtp_payload_internal_h_
#define _rtp_payload_internal_h_

#include "rtp-payload.h"
#include "rtp-packet.h"
#include "rtp-param.h"
#include "rtp-util.h"

// 这里相当于是纯接口，在不同的编码格式下，可以给这四个接口赋予不同的函数，相当于复用（或者是可重入）
// 比如：H264 nalu -> RTP; AAC -> RTP
struct rtp_payload_encode_t   
{
    /// create RTP packer 创建一个RTP封装器
    /// @param[in] size maximum RTP packet payload size(don't include RTP header)
    /// @param[in] payload RTP header PT filed (see more about rtp-profile.h)
    /// @param[in] seq RTP header sequence number filed
    /// @param[in] ssrc RTP header SSRC filed
    /// @param[in] handler user-defined callback
    /// @param[in] cbparam user-defined parameter
    /// @return RTP packer
    void* (*create)(int size, uint8_t payload, uint16_t seq, uint32_t ssrc, struct rtp_payload_t *handler, void* cbparam);
    /// destroy RTP Packer
    void (*destroy)(void* packer);
    // 获取packet的信息
    void (*get_info)(void* packer, uint16_t* seq, uint32_t* timestamp);

    /// PS/H.264 Elementary Stream to RTP Packet, nalu传进来，怎么收到序列化后的rtp，实际是通过回调函数
    /// @param[in] packer
    /// @param[in] data stream data
    /// @param[in] bytes stream length in bytes
    /// @param[in] time stream UTC time
    /// @return 0-ok, ENOMEM-alloc failed, <0-failed
    int (*input)(void* packer, const void* data, int bytes, uint32_t time);
};

struct rtp_payload_decode_t
{
    // 比如： RTP -> H264 nalu; RTP -> AAC 帧
    void* (*create)(struct rtp_payload_t *handler, void* param);
    void (*destroy)(void* packer);

    /// RTP packet to PS/H.264 Elementary Stream
    /// @param[in] decoder RTP packet unpackers
    /// @param[in] packet RTP packet
    /// @param[in] bytes RTP packet length in bytes
    /// @param[in] time stream UTC time
    /// @return 1-packet handled, 0-packet discard, <0-failed
    int (*input)(void* decoder, const void* packet, int bytes);
};


// 通过这四个函数来给上面的结构体赋值
struct rtp_payload_encode_t *rtp_h264_encode(void);

struct rtp_payload_decode_t *rtp_h264_decode(void);

struct rtp_payload_encode_t *rtp_mpeg4_generic_encode(void);

struct rtp_payload_decode_t *rtp_mpeg4_generic_decode(void);


int rtp_packet_serialize_header(const struct RtpPacket *pkt, void* data, int bytes);

#endif /* !_rtp_payload_internal_h_ */
