﻿#ifndef _rtp_payload_h_
#define _rtp_payload_h_

// https://en.wikipedia.org/wiki/RTP_audio_video_profile

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// RTP packet lost(miss packet before this frame)
#define RTP_PAYLOAD_FLAG_PACKET_LOST	0x0100 // some packets lost before the packet
#define RTP_PAYLOAD_FLAG_PACKET_CORRUPT 0x0200 // the packet data is corrupt


// 不管封包还是解包，调用者都是通过回调的方式来获取对应数据（重要的结构体）
struct rtp_payload_t        // 该结构体用来封装回调函数
{
    void* (*alloc)(void* param, int bytes);
    void (*free)(void* param, void *packet);

    /// @return 0-ok, other-error       拿到一帧完整的数据  回调函数 packetCallback
    int (*packet)(void* param, const void *packet, int bytes, uint32_t timestamp, int flags);
};

/// Create RTP packet encoder，创建不同的封装器 (H264 nalu -> RTP; AAC -> RTP)
/// @param[in] payload RTP payload type, value: [0, 127] (see more about rtp-profile.h)
/// @param[in] name RTP payload name
/// @param[in] seq RTP header sequence number filed
/// @param[in] ssrc RTP header SSRC filed
/// @param[in] handler user-defined callback functions
/// @param[in] cbparam user-defined parameter
/// @return NULL-error, other-ok
void* rtp_payload_encode_create(int payload, const char* name, uint16_t seq, uint32_t ssrc, 
                                struct rtp_payload_t *handler, void* cbparam);

void rtp_payload_encode_destroy(void* encoder);

/// Get rtp last packet sequence number and timestamp
/// @param[in] encoder RTP packet encoder(create by rtp_payload_encode_create)
/// @param[in] seq RTP header sequence number
/// @param[in] timestamp RTP header timestamp
void rtp_payload_encode_getinfo(void* encoder, uint16_t* seq, uint32_t* timestamp);

/// Encode RTP packet
/// @param[in] encoder RTP packet encoder(create by rtp_payload_encode_create)
/// @param[in] data stream data
/// @param[in] bytes stream length in bytes
/// @param[in] timestamp RTP header timestamp
/// @return 0-ok, ENOMEM-alloc failed, <0-failed
int rtp_payload_encode_input(void* encoder, const void* data, int bytes, uint32_t timestamp);


/////////////////////////////////////////////////////////////////////////////////////////////
/// Create RTP packet decoder 创建不同的解封装器 (RTP -> H264 nalu; RTP -> AAC 帧)
/// @param[in] payload RTP payload type, value: [0, 127] (see more about rtp-profile.h)
/// @param[in] name RTP payload name
/// @param[in] handler user-defined callback functions
/// @param[in] cbparam user-defined parameter
/// @return NULL-error, other-ok
void* rtp_payload_decode_create(int payload, const char* name, struct rtp_payload_t *handler, void* cbparam);

void rtp_payload_decode_destroy(void* decoder);

/// Decode RTP packet
/// @param[in] decoder RTP packet decoder(create by rtp_payload_decode_create)
/// @param[in] packet RTP packet, include rtp header(12 bytes)
/// @param[in] bytes RTP packet length in bytes
/// @return 1-packet handled, 0-packet discard, <0-failed
int rtp_payload_decode_input(void* decoder, const void* packet, int bytes);


/////////////////////////////////////////////////////////////////////////////////////////////
/// Set/Get rtp encode packet size(include rtp header)
void rtp_packet_setsize(int bytes);
int rtp_packet_getsize(void);

#ifdef __cplusplus
}
#endif
#endif /* !_rtp_payload_h_ */
