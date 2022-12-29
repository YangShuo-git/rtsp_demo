#ifndef _rtp_header_h_
#define _rtp_header_h_

#include <stdint.h>

#define RTP_VERSION 2   // RTP version field must equal 2 (p66)

typedef struct _rtp_header_t
{
    uint32_t v:2;		/* protocol version */
    uint32_t p:1;		/* padding flag */
    uint32_t x:1;		/* header extension flag */
    uint32_t cc:4;		/* CSRC count */
    uint32_t m:1;		/* marker bit */
    uint32_t pt:7;		/* payload type */
    uint32_t seq:16;	/* sequence number */
    uint32_t timestamp; /* timestamp */
    uint32_t ssrc;		/* synchronization source */
} RtpHeader;

#define RTP_V(v)	((v >> 30) & 0x03)
#define RTP_P(v)	((v >> 29) & 0x01)
#define RTP_X(v)	((v >> 28) & 0x01) 
#define RTP_CC(v)	((v >> 24) & 0x0F)
#define RTP_M(v)	((v >> 23) & 0x01)
#define RTP_PT(v)	((v >> 16) & 0x7F)
#define RTP_SEQ(v)	((v >> 00) & 0xFFFF)
#endif /* !_rtp_header_h_ */
