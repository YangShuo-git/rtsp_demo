#include "rtp-packet.h"
#include "rtp-util.h"
#include <string.h>

// RFC3550 RTP: A Transport Protocol for Real-Time Applications
// 5.1 RTP Fixed Header Fields (p12)
/*
 0               1               2               3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|V=2|P|X|   CC  |M|     PT      |      sequence number          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                           timestamp                           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                synchronization source (SSRC) identifier       |
+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
|                 contributing source (CSRC) identifiers        |
|                               ....                            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
// 通过收到的数据，解析出来可读的RTP packet 也就是反序列化
int rtp_packet_deserialize(struct RtpPacket *pkt, const void* data, int bytes)
{
    uint32_t v;
    int headerlen;
    const uint8_t *ptr;

    if (bytes < RTP_FIXED_HEADER_LEN)
    {
        return -1;
    }

    ptr = (const unsigned char *)data;
    memset(pkt, 0, sizeof(struct RtpPacket));

    // pkt header
    v = nbo_r32(ptr);
    pkt->header.v = RTP_V(v);
    pkt->header.p = RTP_P(v);
    pkt->header.x = RTP_X(v);
    pkt->header.cc = RTP_CC(v);
    pkt->header.m = RTP_M(v);
    pkt->header.pt = RTP_PT(v);
    pkt->header.seq = RTP_SEQ(v);
    pkt->header.timestamp = nbo_r32(ptr + 4);
    pkt->header.ssrc = nbo_r32(ptr + 8);

    headerlen = RTP_FIXED_HEADER_LEN + pkt->header.cc * 4;    // header带csrc时，头部总长度
    if (RTP_VERSION != pkt->header.v || bytes < headerlen + (pkt->header.x ? 4 : 0) + (pkt->header.p ? 1 : 0))
    {
        return -1;
    }

    // pkt csrc 
    for (int i = 0; i < pkt->header.cc; i++)
    {
        pkt->csrc[i] = nbo_r32(ptr + 12 + i * 4);
    }

    // 跳过头部 拿到payload与payloadlen；如果有拓展，则在下面减掉
    pkt->payload = (uint8_t*)ptr + headerlen;      
    pkt->payloadlen = bytes - headerlen;           

    // pkt header extension
    if (1 == pkt->header.x)
    {
        const uint8_t *rtpext = ptr + headerlen;
        pkt->extension = rtpext + 4;
        pkt->reserved = nbo_r16(rtpext);
        pkt->extlen = nbo_r16(rtpext + 2) * 4;
        if (pkt->extlen + 4 > pkt->payloadlen)
        {
            return -1;
        }
        else
        {
            pkt->payload = rtpext + pkt->extlen + 4;
            pkt->payloadlen -= pkt->extlen + 4;
        }
    }

    // padding
    if (1 == pkt->header.p)
    {
        uint8_t padding = ptr[bytes - 1];
        if (pkt->payloadlen < padding)
        {
            return -1;
        }
        else
        {
            pkt->payloadlen -= padding;
        }
    }

    return 0;
}

// 把可读RTP packet封装成要发送出去的数据  也就是序列化
int rtp_packet_serialize_header(const struct RtpPacket *pkt, void* data, int bytes)
{
    int headerlen;
    uint8_t* ptr;

    // RTP version field must equal 2 (p66)
    if (RTP_VERSION != pkt->header.v || 0 != (pkt->extlen % 4))
    {
        return -1;
    }

    // RFC3550 5.1 RTP Fixed Header Fields(p12)
    headerlen = RTP_FIXED_HEADER_LEN + pkt->header.cc * 4 + (pkt->header.x ? 4 : 0);
    if (bytes < headerlen + pkt->extlen)
    {
        return -1;
    }

    ptr = (uint8_t *)data;

    // write 12字节的 fixed header
    nbo_write_rtp_header(ptr, &pkt->header);
    ptr += RTP_FIXED_HEADER_LEN;

    // write CSRC
    for (int i = 0; i < pkt->header.cc; i++, ptr += 4)
    {
        nbo_w32(ptr, pkt->csrc[i]);     // csrc列表封装到头部
    }

    // write header extension
    if (1 == pkt->header.x)
    {
        // 5.3.1 RTP Header Extension
        nbo_w16(ptr, pkt->reserved);
        nbo_w16(ptr + 2, pkt->extlen / 4);
        memcpy(ptr + 4, pkt->extension, pkt->extlen);   // extension封装到头部
        ptr += pkt->extlen + 4;
    }

    return headerlen + pkt->extlen;
}

int rtp_packet_serialize(const struct RtpPacket *pkt, void* data, int bytes)
{
    int headerlen;

    headerlen = rtp_packet_serialize_header(pkt, data, bytes);
    if (headerlen < RTP_FIXED_HEADER_LEN || headerlen + pkt->payloadlen > bytes)
        return -1;

    memcpy(((uint8_t*)data) + headerlen, pkt->payload, pkt->payloadlen);
    return headerlen + pkt->payloadlen;
}