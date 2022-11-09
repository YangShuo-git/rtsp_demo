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



struct rtp_h264_test_t
{
    int payload;                // payload type
    const char* encoding;       // 音频、视频的格式，比如H264, 该工程只支持264
    int fd;
    struct sockaddr_in addr;
    size_t addr_size;

    char *in_file_name;             // H264文件名
    FILE* in_file;                  // H264裸流文件
    float frame_rate;               // 帧率

    void* encoder_h264;             // 封装

    char *out_file_name;
    FILE *out_file;
    void* decoder_h264;             // 解封装
    uint8_t sps[40];
    int sps_len;
    uint8_t pps[10];
    int pps_len;
    int got_sps_pps;
};

#if 0
static void* rtp_alloc(void*  param, int bytes)
{
    static uint8_t buffer[2 * 1024 * 1024 + 4] = { 0, 0, 0, 1, };   // 支持2M大小，不包括start code
    assert(bytes <= sizeof(buffer) - 4);
    return buffer + 4;
}

static void rtp_free(void* param, void * packet)    // 因为rtp_alloc是静态分配所以不需要释放
{
}

// 拿到一帧RTP序列化后的数据
static int rtp_encode_packet(void* param, const void *packet, int bytes, uint32_t timestamp, int flags)
{
    struct rtp_h264_test_t* ctx = (struct rtp_h264_test_t*)param;
    int ret = 0;
    //1. 通过socket发送出去
    ret = sendto(ctx->fd,
                (void*)packet,
                bytes,
                0,
                (struct sockaddr*)&ctx->addr,
                ctx->addr_size);
    uint8_t *nalu = (uint8_t *)packet;
    printf("rtp send packet -> nalu_type:%d, 0x%02x, 0x%02x, bytes:%d, timestamp:%u\n",
           nalu[12]&0x1f,  nalu[12],  nalu[13], bytes, timestamp);

    //2. 解封装，用于保存为裸流h264（存疑?)
    ret = rtp_payload_decode_input(ctx->decoder_h264, packet, bytes);       // 解封装

    return 0;
}

static int rtp_decode_packet(void* param, const void *packet, int bytes, uint32_t timestamp, int flags)
{
    static const uint8_t start_code[4] = { 0, 0, 0, 1 };
    struct rtp_h264_test_t* ctx = (struct rtp_h264_test_t*)param;

    static uint8_t buffer[2 * 1024 * 1024];
    assert(bytes + 4 < sizeof(buffer));
    assert(0 == flags);

    size_t size = 0;
    if (0 == strcmp("H264", ctx->encoding) || 0 == strcmp("H265", ctx->encoding))
    {
        memcpy(buffer, start_code, sizeof(start_code));
        size += sizeof(start_code);
    }
    else if (0 == strcasecmp("mpeg4-generic", ctx->encoding))
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
    printf("nalu get -> bytes:%d, timestamp:%u\n", size, timestamp);
    // TODO:
    // check media file
    fwrite(buffer, 1, size, ctx->out_file);
}
#endif

// 发送H264 RTP over UDP
#define DEST_IP              "192.168.2.110"      // 支持成对方的ip地址
#define DEST_PORT            9832   //端口号

// ffplay 播放 ffplay h264.sdp -protocol_whitelist "file,http,https,rtp,udp,tcp,tls"

int main()
{
    printf("main function!\n");
    return 0;
    #if 0
    FILE *bits = open_bitstream_file("phone.h264");//打开264文件，并将文件指针赋给bits,在此修改文件名实现打开别的264文件。
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

    struct rtp_h264_test_t ctx;     // 封装的测试 带H264 RTP封装和解封装
    memset(&ctx, 0, sizeof(struct rtp_h264_test_t));

    ctx.in_file = bits;             // 输入文件
    ctx.out_file = out_file;        // 输出文件

    usleep(25000);


    printf("close file\n");
    fclose(ctx.in_file);
    fclose(ctx.out_file);
    #endif
}
