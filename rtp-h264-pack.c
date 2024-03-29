﻿// RFC6184 RTP Payload Format for H.264 Video
//
// 6.2. Single NAL Unit Mode (All receivers MUST support this mode)
//   packetization-mode media type parameter is equal to 0 or the packetization - mode is not present.
//   Only single NAL unit packets MAY be used in this mode.
//   STAPs, MTAPs, and FUs MUST NOT be used.
//   The transmission order of single NAL unit packets MUST comply with the NAL unit decoding order.
// 6.3. Non-Interleaved Mode (This mode SHOULD be supported)
//   packetization-mode media type parameter is equal to 1.
//   Only single NAL unit packets, STAP - As, and FU - As MAY be used in this mode.
//   STAP-Bs, MTAPs, and FU-Bs MUST NOT be used.
//   The transmission order of NAL units MUST comply with the NAL unit decoding order
// 6.4. Interleaved Mode
//   packetization-mode media type parameter is equal to 2.
//   STAP-Bs, MTAPs, FU-As, and FU-Bs MAY be used.
//   STAP-As and single NAL unit packets MUST NOT be used.
//   The transmission order of packets and NAL units is constrained as specified in Section 5.5.
//
// 5.1. RTP Header Usage (p10)
// The RTP timestamp is set to the sampling timestamp of the content. A 90 kHz clock rate MUST be used.

#include "rtp-packet.h"
#include "rtp-payload-internal.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#define KHz         90 // 90000Hz
#define FU_START    0x80
#define FU_END      0x40

#define N_FU_HEADER	2

// 二层  真正实现的接口

struct RtpH264Packer
{
    int size;
    struct RtpPacket pkt;

    struct rtp_payload_t handler;
    void* cbparam;
};

static void* rtp_h264_pack_create(int size, uint8_t pt, uint16_t seq, uint32_t ssrc, struct rtp_payload_t *handler, void* cbparam)
{
    struct RtpH264Packer *packer;
    packer = (struct RtpH264Packer *)calloc(1, sizeof(*packer));
    if(!packer) return NULL;

    packer->size = size;
    packer->pkt.header.v = RTP_VERSION;
    packer->pkt.header.pt = pt;
    packer->pkt.header.seq = seq;
    packer->pkt.header.ssrc = ssrc;

    memcpy(&packer->handler, handler, sizeof(packer->handler));
    packer->cbparam = cbparam;
    // 至此，RTP包packer创建成功，两个在main.c的结构体：rtp_payload_t、RtpContext，也透传到最底层了

    return packer;
}

static void rtp_h264_pack_destroy(void* pack)
{
    struct RtpH264Packer *packer;
    packer = (struct RtpH264Packer *)pack;
#if defined(_DEBUG) || defined(DEBUG)
    memset(packer, 0xCC, sizeof(*packer));
#endif
    free(packer);
}

static void rtp_h264_pack_get_info(void* pack, uint16_t* seq, uint32_t* timestamp)
{
    struct RtpH264Packer *packer;
    packer = (struct RtpH264Packer *)pack;
    *seq = (uint16_t)packer->pkt.header.seq;
    *timestamp = packer->pkt.header.timestamp;
}

static const uint8_t* h264_nalu_find(const uint8_t* p, const uint8_t* end)
{
    for(p += 2; p + 1 < end; p++)
    {
        if (0x01 == *p && 0x00 == *(p - 1) && 0x00 == *(p - 2))
            return p + 1;
    }
    return end;
}

static int rtp_h264_pack_nalu(struct RtpH264Packer *packer, const uint8_t* nalu, int bytes)
{
    int r, len;
    uint8_t *rtp;

    packer->pkt.payload = nalu;
    packer->pkt.payloadlen = bytes;
    len = RTP_FIXED_HEADER_LEN + packer->pkt.payloadlen;
    rtp = (uint8_t*)packer->handler.alloc(packer->cbparam, len);
    if (!rtp) return ENOMEM;

    //packer->pkt.rtp.m = 1; // set marker flag
    packer->pkt.header.m = (*nalu & 0x1f) <= 5 ? 1 : 0; // VCL only
    len = rtp_packet_serialize(&packer->pkt, rtp, len);  // 序列化
    if (len != RTP_FIXED_HEADER_LEN + packer->pkt.payloadlen)
    {
        return -1;
    }

    ++packer->pkt.header.seq;
    r = packer->handler.packetCallback(packer->cbparam, rtp, len, packer->pkt.header.timestamp, 0); // 通过packet回调函数把封包好的rtp包发送到应用层
    packer->handler.free(packer->cbparam, rtp);
    return r;
}

static int rtp_h264_pack_fu_a(struct RtpH264Packer *packer, const uint8_t* nalu, int bytes)
{
    int r, n;
    unsigned char *rtp;

    // RFC6184 5.3. NAL Unit Header Usage: Table 2 (p15)
    // RFC6184 5.8. Fragmentation Units (FUs) (p29)
    uint8_t fu_indicator = (*nalu & 0xE0) | 28; // FU-A 固定的逻辑
    uint8_t fu_header = *nalu & 0x1F;           // 设置为s置位1

    r = 0;
    nalu += 1; // skip NAL Unit Type byte
    bytes -= 1;
    assert(bytes > 0);

    // FU-A start
    for (fu_header |= FU_START; 0 == r && bytes > 0; ++packer->pkt.header.seq)
    {
        if (bytes + RTP_FIXED_HEADER_LEN <= packer->size - N_FU_HEADER)
        {
            assert(0 == (fu_header & FU_START));
            fu_header = FU_END | (fu_header & 0x1F); // FU-A end  整个nalu结束了
            packer->pkt.payloadlen = bytes;
        }
        else
        {
            packer->pkt.payloadlen = packer->size - RTP_FIXED_HEADER_LEN - N_FU_HEADER;
        }

        packer->pkt.payload = nalu;
        n = RTP_FIXED_HEADER_LEN + N_FU_HEADER + packer->pkt.payloadlen;
        rtp = (uint8_t*)packer->handler.alloc(packer->cbparam, n);
        if (!rtp) return -ENOMEM;

        packer->pkt.header.m = (FU_END & fu_header) ? 1 : 0; // set marker flag
        n = rtp_packet_serialize_header(&packer->pkt, rtp, n);
        if (n != RTP_FIXED_HEADER_LEN)
        {
            assert(0);
            return -1;
        }

        /*fu_indicator + fu_header*/
        rtp[n + 0] = fu_indicator;
        rtp[n + 1] = fu_header;
        memcpy(rtp + n + N_FU_HEADER, packer->pkt.payload, packer->pkt.payloadlen);
        // packer->cbparam用户的参数
        r = packer->handler.packetCallback(packer->cbparam, rtp, n + N_FU_HEADER + packer->pkt.payloadlen, packer->pkt.header.timestamp, 0);
        packer->handler.free(packer->cbparam, rtp);

        bytes -= packer->pkt.payloadlen;
        nalu += packer->pkt.payloadlen;
        fu_header &= 0x1F; // clear flags
    }

    return r;
}

// 这里读进来的h264数据是有startcode
static int rtp_h264_pack_input(void* pack, const void* h264, int bytes, uint32_t timestamp)
{
    int r = 0;
    const uint8_t *p1, *p2, *pend;
    struct RtpH264Packer *packer;
    packer = (struct RtpH264Packer *)pack;
    packer->pkt.header.timestamp = timestamp; //(uint32_t)time * KHz; // ms -> 90KHZ

    pend = (const uint8_t*)h264 + bytes;
    for(p1 = h264_nalu_find((const uint8_t*)h264, pend); 0 == r && p1 < pend; p1 = p2)
    {
        size_t nalu_size;

        // filter H.264 start code(0x000001或者0x00000001)
        p2 = h264_nalu_find(p1 + 1, pend);
        nalu_size = p2 - p1; // 过滤掉起始码后，nalu的长度

        // filter suffix '00' bytes  （为什么这样做？）
        if (p2 != pend) --nalu_size;
        while(0 == p1[nalu_size-1]) --nalu_size;

        if(nalu_size + RTP_FIXED_HEADER_LEN <= (size_t)packer->size)    // 小于等于payload size的
        {
            // single NAl unit packet
            r = rtp_h264_pack_nalu(packer, p1, (int)nalu_size);     // 直接打包
        }
        else
        {
            r = rtp_h264_pack_fu_a(packer, p1, (int)nalu_size);     // FU-A打包
        }
    }

    return r;
}

// 该函数返回一个结构体，该结构体包含4个回调函数，用来处理h264的RTP封装
struct rtp_payload_encode_t *rtp_h264_encode()
{
    // 这里是给回调函数赋值(可重入)
    static struct rtp_payload_encode_t encoder = {
        rtp_h264_pack_create,
        rtp_h264_pack_destroy,
        rtp_h264_pack_get_info,
        rtp_h264_pack_input,
    };

    return &encoder;
}
