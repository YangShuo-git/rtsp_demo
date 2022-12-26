#include "rtp-payload.h"
//#include "rtp-profile.h"
#include "rtp-packet.h"
#include "rtp-payload-internal.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define TS_PACKET_SIZE 188

struct rtp_payload_delegate_t     // 代理结构体
{
    struct rtp_payload_encode_t* encoder;
    struct rtp_payload_decode_t* decoder;
    void* packer;
};

/// @return 0-ok, <0-error
static int rtp_payload_find(int payload, const char* format, struct rtp_payload_delegate_t* codec);

/**
 * @brief rtp_payload_encode_create
 * @param payload  媒体的类型
 * @param name     对应的编码器 H264/H265
 * @param seq
 * @param ssrc
 * @param handler
 * @param cbparam
 * @return
 */
void* rtp_payload_encode_create(int payload, const char* name, uint16_t seq, uint32_t ssrc, struct rtp_payload_t *handler, void* cbparam)
{
    int size;
    struct rtp_payload_delegate_t* delegateCtx;

    delegateCtx = calloc(1, sizeof(*delegateCtx));
    if (delegateCtx)
    {
        size = rtp_packet_getsize();
        if (rtp_payload_find(payload, name, delegateCtx) < 0   // 查找有没有注册该封装器
            || NULL == (delegateCtx->packer = delegateCtx->encoder->create(size, (uint8_t)payload, seq, ssrc, handler, cbparam)))
        {
            free(delegateCtx);
            return NULL;
        }
    }
    return delegateCtx;
}

void rtp_payload_encode_destroy(void* encoder)
{
    struct rtp_payload_delegate_t* delegateCtx;
    delegateCtx = (struct rtp_payload_delegate_t*)encoder;
    delegateCtx->encoder->destroy(delegateCtx->packer);
    free(delegateCtx);
}

void rtp_payload_encode_getinfo(void* encoder, uint16_t* seq, uint32_t* timestamp)
{
    struct rtp_payload_delegate_t* delegateCtx;
    delegateCtx = (struct rtp_payload_delegate_t*)encoder;
    delegateCtx->encoder->get_info(delegateCtx->packer, seq, timestamp);
}

/**
 * @brief rtp_payload_encode_input 这里是通用的接口   在这里注册了重要的回调函数
 * @param encoder
 * @param data      data具体是什么媒体类型的数据，接口不关注，具体由ctx->encoder->input去处理 (rtp_h264_pack_input)
 * @param bytes
 * @param timestamp
 * @return
 */
int rtp_payload_encode_input(void* encoder, const void* data, int bytes, uint32_t timestamp)
{
    struct rtp_payload_delegate_t* delegateCtx;
    delegateCtx = (struct rtp_payload_delegate_t*)encoder;
    return delegateCtx->encoder->input(delegateCtx->packer, data, bytes, timestamp);
}


void* rtp_payload_decode_create(int payload, const char* name, struct rtp_payload_t *handler, void* cbparam)
{
    struct rtp_payload_delegate_t* delegateCtx;
    delegateCtx = calloc(1, sizeof(*delegateCtx));
    if (delegateCtx)
    {
        if (rtp_payload_find(payload, name, delegateCtx) < 0
            || NULL == (delegateCtx->packer = delegateCtx->decoder->create(handler, cbparam)))
        {
            free(delegateCtx);
            return NULL;
        }
    }
    return delegateCtx;
}

void rtp_payload_decode_destroy(void* decoder)
{
    struct rtp_payload_delegate_t* delegateCtx;
    delegateCtx = (struct rtp_payload_delegate_t*)decoder;
    delegateCtx->decoder->destroy(delegateCtx->packer);
    free(delegateCtx);
}

int rtp_payload_decode_input(void* decoder, const void* packet, int bytes)
{
    struct rtp_payload_delegate_t* delegateCtx;
    delegateCtx = (struct rtp_payload_delegate_t*)decoder;
    return delegateCtx->decoder->input(delegateCtx->packer, packet, bytes);
}

// Default max packet size (1500, minus allowance for IP, UDP, UMTP headers)
// (Also, make it a multiple of 4 bytes, just in case that matters.)
//static int s_max_packet_size = 1456; // from Live555 MultiFrameRTPSink.cpp RTP_PAYLOAD_MAX_SIZE
//static size_t s_max_packet_size = 576; // UNIX Network Programming by W. Richard Stevens
static int s_max_packet_size = 1434; // from VLC

void rtp_packet_setsize(int bytes)
{
    s_max_packet_size = bytes < 564 ? 564 : bytes;
}

int rtp_packet_getsize()
{
    return s_max_packet_size;
}

static int rtp_payload_find(int payload, const char* format, struct rtp_payload_delegate_t* codec)
{
    if (payload >= 96 && format)
    {
        if (0 == strcasecmp(format, "H264"))
        {
            // H.264 video (MPEG-4 Part 10) (RFC 6184)
            codec->encoder = rtp_h264_encode();
            codec->decoder = rtp_h264_decode();
        }
        else
        {
            return -1;
        }
    }
    else
    {
        return -1;
    }

    return 0;
}
