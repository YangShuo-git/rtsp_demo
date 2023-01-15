#include <stdlib.h>
#include <string.h>
#include "rtp-payload.h"
#include "rtp-profile.h"
#include "rtp-packet.h"
#include "rtp-payload-internal.h"

#define TS_PACKET_SIZE 188

// 一层  代理接口

/// @return 0-ok, <0-error
static int rtp_payload_find(int payloadType, const char* format, struct RtpPayloadDelagate* delegateCtx);

/**
 * @brief rtp_payload_encode_create
 * @param payloadType  媒体的类型  96是采用PS解复用，将音视频分开解码；98是直接按照H264的解码类型解码
 * @param name     编码器名字 H264/H265
 * @param seq
 * @param ssrc
 * @param handler
 * @param cbparam
 * @return
 */
void* rtp_payload_encode_create(int payloadType, const char* name, uint16_t seq, uint32_t ssrc, struct rtp_payload_t *handler, void* cbparam)
{
    int size;
    struct RtpPayloadDelagate* delegateCtx;

    delegateCtx = calloc(1, sizeof(*delegateCtx));
    if (delegateCtx)
    {
        size = rtp_packet_getsize();
        if (rtp_payload_find(payloadType, name, delegateCtx) < 0   // 其实就是注册代理结构体里的结构体encoder、decoder（二层接口，可重入）
            || NULL == (delegateCtx->packer = delegateCtx->encoder->create(size, (uint8_t)payloadType, seq, ssrc, handler, cbparam)))
        {
            free(delegateCtx);
            return NULL;
        }
    }
    return delegateCtx;
}

void rtp_payload_encode_destroy(void* encoder)
{
    struct RtpPayloadDelagate* delegateCtx;
    delegateCtx = (struct RtpPayloadDelagate*)encoder;
    delegateCtx->encoder->destroy(delegateCtx->packer);
    free(delegateCtx);
}

void rtp_payload_encode_getinfo(void* encoder, uint16_t* seq, uint32_t* timestamp)
{
    struct RtpPayloadDelagate* delegateCtx;
    delegateCtx = (struct RtpPayloadDelagate*)encoder;
    delegateCtx->encoder->get_info(delegateCtx->packer, seq, timestamp);
}

/**
 * @brief rtp_payload_encode_input 这里是通用的接口   在这里注册了重要的回调函数
 * @param encoder
 * @param data      data具体是什么媒体类型的数据，接口不关注，具体由ctx->encoder->input去处理 （比如rtp_h264_pack_input)
 * @param bytes
 * @param timestamp
 * @return
 */
int rtp_payload_encode_input(void* encoder, const void* data, int bytes, uint32_t timestamp)
{
    struct RtpPayloadDelagate* delegateCtx;
    delegateCtx = (struct RtpPayloadDelagate*)encoder;
    
    return delegateCtx->encoder->input(delegateCtx->packer, data, bytes, timestamp);
}




void* rtp_payload_decode_create(int payloadType, const char* name, struct rtp_payload_t *handler, void* cbparam)
{
    struct RtpPayloadDelagate* delegateCtx;
    delegateCtx = calloc(1, sizeof(*delegateCtx));
    if (delegateCtx)
    {
        if (rtp_payload_find(payloadType, name, delegateCtx) < 0
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
    struct RtpPayloadDelagate* delegateCtx;
    delegateCtx = (struct RtpPayloadDelagate*)decoder;
    delegateCtx->decoder->destroy(delegateCtx->packer);
    free(delegateCtx);
}

int rtp_payload_decode_input(void* decoder, const void* packet, int bytes)
{
    struct RtpPayloadDelagate* delegateCtx;
    delegateCtx = (struct RtpPayloadDelagate*)decoder;
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


// 注册delegateCtx中的两个结构体rtp_payload_encode_t、rtp_payload_decode_t
static int rtp_payload_find(int payloadType, const char* format, struct RtpPayloadDelagate* delegateCtx)
{
    if (payloadType >= 96 && format)
    {
        if (0 == strcasecmp(format, "H264"))
        {
            // H.264 video (MPEG-4 Part 10) (RFC 6184)
            delegateCtx->encoder = rtp_h264_encode();
            delegateCtx->decoder = rtp_h264_decode();
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
