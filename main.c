#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>

#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include "rtp-payload.h"
#include "h264-util.h"

// 贯穿整个主线的ctx
struct RtpContext
{
    int payloadType;           // RFC2250 建议96 表示PS 封装，建议97 为MPEG-4，建议98 为H264
    const char* format;        // 音频、视频的格式，比如H264, 该工程暂时只支持264

    int fd;                    // socket的返回值
    struct sockaddr_in addr;   // 结构体里面保存了IP地址和端口号
    size_t addr_size;          // addr的大小

    char* in_file_name;             // H264文件名
    FILE* in_file;                  // H264裸流文件
    float frame_rate;               // 帧率  是手动设置的

    void* encoder_h264;             // 代理封装
    void* decoder_h264;             // 代理解封装

    char *out_file_name;
    FILE *out_file;

    uint8_t sps[40];
    int sps_len;
    uint8_t pps[10];
    int pps_len;
    int got_sps_pps;
};

static void* rtp_alloc(void* param, int bytes)
{
    static uint8_t buffer[2 * 1024 * 1024 + 4] = { 0, 0, 0, 1, };   // 支持2M大小，不包括start code
    assert(bytes <= sizeof(buffer) - 4);
    return buffer + 4;
}

static void rtp_free(void* param, void* packet)    // 因为rtp_alloc是静态分配所以不需要释放
{
}

// 拿到一帧RTP序列化后的数据
static int rtp_encode_packet(void* param, const void* packet, int bytes, uint32_t timestamp, int flags)
{
    struct RtpContext* ctx = (struct RtpContext*)param;
    int ret = 0;
    //1. 通过socket发送出去
    ret = sendto(ctx->fd,
                (void*)packet,
                bytes,
                0,
                (struct sockaddr*)&ctx->addr,
                ctx->addr_size);
    uint8_t *nalu = (uint8_t *)packet;
    printf("Send rtp packet: nalu_type:%d, 0x%02x, 0x%02x, bytes:%d, timestamp:%u\n",
           nalu[12]&0x1f,  nalu[12],  nalu[13], bytes, timestamp);

    //2. 解封装，用于保存为裸流h264
    ret = rtp_payload_decode_input(ctx->decoder_h264, packet, bytes);

    return 0;
}

static int rtp_decode_packet(void* param, const void* packet, int bytes, uint32_t timestamp, int flags)
{
    static const uint8_t start_code[4] = { 0, 0, 0, 1 };
    struct RtpContext* ctx = (struct RtpContext*)param;

    static uint8_t buffer[2 * 1024 * 1024];
    assert(bytes + 4 < sizeof(buffer));
    assert(0 == flags);

    size_t size = 0;
    if (0 == strcmp("H264", ctx->format) || 0 == strcmp("H265", ctx->format))
    {
        memcpy(buffer, start_code, sizeof(start_code));
        size += sizeof(start_code);
    }
    else if (0 == strcasecmp("mpeg4-generic", ctx->format))
    {
        int len = bytes + 7;
        uint8_t profile = 2;
        uint8_t sampling_frequency_index = 4;
        uint8_t channel_configuration = 2;
        buffer[0] = 0xFF; /* 12-syncword */
        buffer[1] = 0xF0 /* 12-syncword */ | (0 << 3)/*1-ID*/ | (0x00 << 2) /*2-layer*/ | 0x01 /*1-protection_absent*/;
        buffer[2] = ((profile - 1) << 6) | ((sampling_frequency_index & 0x0F) << 2) | ((channel_configuration >> 2) & 0x01);
        buffer[3] = ((channel_configuration & 0x03) << 6) | ((len >> 11) & 0x03); /*0-original_copy*/ /*0-home*/ /*0-copyright_identification_bit*/ /*0-copyright_identification_start*/
        buffer[4] = (uint8_t)(len >> 3);
        buffer[5] = ((len & 0x07) << 5) | 0x1F;
        buffer[6] = 0xFC | ((len / 1024) & 0x03);
        size = 7;
    }
    memcpy(buffer + size, packet, bytes);
    size += bytes;
    printf("Get nalu: bytes:%d, timestamp:%u\n", size, timestamp);
    // TODO:
    // check media file
    fwrite(buffer, 1, size, ctx->out_file);
}

// 发送H264 RTP over UDP
#define DEST_IP              "192.168.205.128"      // 支持成对方的ip地址
#define DEST_PORT            9832   //端口号

// ffplay播放 ffplay h264.sdp -protocol_whitelist "file,http,https,rtp,udp,tcp,tls"

int main()
{
    FILE *bits = open_bitstream_file("phone.h264"); 
    if(!bits)
    {
        printf("open file failed\n");
        return -1;
    }
    FILE *out_file = fopen("out.h264", "wb");
    if(!out_file)
    {
        printf("open out_file failed\n");
        return -1;
    }

    nalu_t *oneNalu = NULL;
    struct RtpContext rtpCtx;     // 封装的测试 带H264 RTP封装和解封装
    memset(&rtpCtx, 0, sizeof(struct RtpContext));

    rtpCtx.in_file = bits;         // 输入文件
    rtpCtx.out_file = out_file;    // 输出文件

    // H264 RTP encode回调
    struct rtp_payload_t handler_rtp_encode_h264;
    handler_rtp_encode_h264.alloc = rtp_alloc;
    handler_rtp_encode_h264.free = rtp_free;
    handler_rtp_encode_h264.packet = rtp_encode_packet;

    rtpCtx.payloadType = 96;  // 采用PS解复用，将音视频分开解码
    rtpCtx.format = "H264";
    rtpCtx.encoder_h264 = rtp_payload_encode_create(rtpCtx.payloadType, rtpCtx.format, 1, 0x12345678, &handler_rtp_encode_h264, &rtpCtx);

    // H264 RTP decode回调
    struct rtp_payload_t handler_rtp_decode_h264;
    handler_rtp_decode_h264.alloc = rtp_alloc;
    handler_rtp_decode_h264.free = rtp_free;
    handler_rtp_decode_h264.packet = rtp_decode_packet;
    rtpCtx.decoder_h264 = rtp_payload_decode_create(rtpCtx.payloadType, rtpCtx.format, &handler_rtp_decode_h264, &rtpCtx);


    rtpCtx.frame_rate = 25;
    unsigned int timestamp_increse = 0;
    unsigned int ts_current = 0;
    timestamp_increse = (unsigned int)(90000.0 / rtpCtx.frame_rate);  //时间戳的增量（每帧的采样点数） 采样率/帧率


    rtpCtx.addr.sin_family = AF_INET;
    rtpCtx.addr.sin_addr.s_addr = inet_addr(DEST_IP);
    rtpCtx.addr.sin_port = htons(DEST_PORT);
    rtpCtx.fd = socket(AF_INET, SOCK_DGRAM, 0);
#if 0
    int ret = fcntl(rtpCtx.fd , F_GETFL , 0);
    fcntl(rtpCtx.fd , F_SETFL , ret | O_NONBLOCK);
#endif
    rtpCtx.addr_size =sizeof(rtpCtx.addr);
    // connect(rtpCtx.fd, (const sockaddr *)&rtpCtx.addr, len) ;//申请UDP套接字


    oneNalu = alloc_nalu(2000000); //为nalu_t及其成员buf分配空间，2M分配给了buf。返回值为指向nalu_t存储空间的指针
    // 这里为什么会分配2M内存呢？

    while(!feof(bits))  //如果文件到头，则返回非0值；文件没有到头，返回0
    {
        int ret = get_annexb_nalu(oneNalu, bits); //每执行一次，文件的指针指向本次找到的NALU的末尾，下一个位置即为下个NALU的起始码
        printf("Read h264bitstram: nal_unit_type:%d, unit_len:%d\n", oneNalu->nal_unit_type, oneNalu->len);

        if(oneNalu->nal_unit_type == 7 && rtpCtx.got_sps_pps == 0)
        {
            memcpy(rtpCtx.sps, &oneNalu->buf[oneNalu->startCodeLen], oneNalu->len - oneNalu->startCodeLen);
            rtpCtx.sps_len = oneNalu->len - oneNalu->startCodeLen;
        }
        if(oneNalu->nal_unit_type == 8 && rtpCtx.got_sps_pps == 0)
        {
            memcpy(rtpCtx.pps, &oneNalu->buf[oneNalu->startCodeLen], oneNalu->len - oneNalu->startCodeLen);
            rtpCtx.pps_len = oneNalu->len - oneNalu->startCodeLen;
            if(!rtpCtx.got_sps_pps)
            {
                h264_sdp_create("h264.sdp", DEST_IP, DEST_PORT,
                                rtpCtx.sps, rtpCtx.sps_len,
                                rtpCtx.pps, rtpCtx.pps_len,
                                rtpCtx.payloadType, 90000, 300000);
                rtpCtx.got_sps_pps = 1;            // 只发送一次
            }

        }

        // 这里是输入一个nalu，去进行RTP封包
        ret = rtp_payload_encode_input(rtpCtx.encoder_h264, oneNalu->buf, oneNalu->len, ts_current);
        if(oneNalu->nal_unit_type != 7 && oneNalu->nal_unit_type != 8 && oneNalu->nal_unit_type != 6) 
        {
            ts_current = ts_current + timestamp_increse;   // 注意时间戳的完整一帧数据后再叠加（不是slice，一帧可能有多个slice）
        }

        usleep(25000);
    }

    printf("close file\n");
    fclose(rtpCtx.in_file);
    fclose(rtpCtx.out_file);
}
