#ifndef _rtp_packet_h_
#define _rtp_packet_h_

#include "rtp-header.h"

#define RTP_FIXED_HEADER 12     // RTP的header，固定长度

// RTP包 包括 header + [csrc/extension] + payload
struct RtpPacket     
{
    RtpHeader header;

    uint32_t csrc[16];      // 最多16个csrc
    const void* extension;  // extension(valid only if rtp.x = 1)
    uint16_t extlen;        // extension length in bytes
    uint16_t reserved;      // extension reserved

    const void* payload; // rtp payload
    int payloadlen;      // payload length in bytes
};

///@return 0-ok, other-error   解包
int rtp_packet_deserialize(struct RtpPacket *pkt, const void* data, int bytes);

///@return <0-error, >0-rtp packet size, =0-impossible  封包
int rtp_packet_serialize(const struct RtpPacket *pkt, void* data, int bytes);

#endif /* !_rtp_packet_h_ */
