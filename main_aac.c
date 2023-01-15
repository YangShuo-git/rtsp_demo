#include <stdio.h>
#include <string.h>
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
#include "aac-util.h"

// 贯穿整个主线的ctx
struct RtpContext
{
    int payloadType;           // RFC2250 96表示PS封装，97为MPEG-4，98为H264
    const char* format;        // 音频、视频的格式，比如aac

    int fd;                    // socket的返回值
    struct sockaddr_in addr;   // 结构体里面保存了IP地址和端口号
    size_t addrSize;          // addr的大小

    char* inFileName;             // 文件名
    FILE* inFile;                  // 裸流文件

    char *outFileName;
    FILE *outFile;


    // aac相关的参数  这里的aac用 mpeg4-generic
    int profile;
    int sampling_frequency_index;      // 采样率
    int channel_configuration;

    void* encoder_aac;  
    void* decoder_aac;
};

static void* rtp_alloc(void* param, int bytes)
{
    static uint8_t buffer[2 * 1024 * 1024 + 4] = { 0, 0, 0, 1, };   // 支持2M大小，不包括start code
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
                MSG_DONTWAIT,
                (struct sockaddr*)&ctx->addr,
                ctx->addrSize);
    printf("\nret:%d, rtp send packet -> bytes:%d, timestamp:%u\n", ret, bytes, timestamp);

    //2. 解封装，用于保存为裸流h264
    ret = rtp_payload_decode_input(ctx->decoder_aac, packet, bytes);

    return 0;
}

static int rtp_decode_packet(void* param, const void* packet, int bytes, uint32_t timestamp, int flags)
{
    struct RtpContext* ctx = (struct RtpContext*)param;
    static uint8_t buffer[2 * 1024 * 1024];

    size_t size = 0;
    if (0 == strcasecmp("mpeg4-generic", ctx->format))
    {
        int len = bytes + 7;
        uint8_t profile = ctx->profile;
        uint8_t sampling_frequency_index = ctx->sampling_frequency_index;       // 本质上这些是从sdp读取的
        uint8_t channel_configuration = ctx->channel_configuration;
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
    printf("aac get -> bytes:%d, timestamp:%u\n", size, timestamp);
    // TODO:
    // check media file
    fwrite(buffer, 1, size, ctx->outFile);
}

// 这里目的是为了让音频实时去发送，获取的时间是ms
int64_t get_current_time() {
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return ((unsigned long long)tv.tv_sec * 1000 + (long)tv.tv_usec / 1000);
}

// 发送RTP over UDP
#define DEST_IP              "192.168.205.128"  // 支持成对方的ip地址
#define DEST_PORT            9832   //端口号

// ffplay播放 ffplay aac.sdp -protocol_whitelist "file,http,https,rtp,udp,tcp,tls"
// ffplay播放 ffplay h264.sdp -protocol_whitelist "file,http,https,rtp,udp,tcp,tls" -loglevel 58

int main1()
{
    //1.打开本地输入文件，这个文件是要实时发送的
    FILE *bits = aac_open_bitstream_file("in.aac"); 
    if(!bits)
    {
        printf("open file failed\n");
        return -1;
    }
    //2.解包后保存的裸流文件，主要测试对比in.aac文件是否一致
    FILE *out_file = fopen("out.aac", "wb");
    if(!out_file)
    {
        printf("open out_file failed\n");
        return -1;
    }

    struct RtpContext rtpCtx;     // 封装的测试  RTP封装和解封装
    memset(&rtpCtx, 0, sizeof(struct RtpContext));

    rtpCtx.inFile = bits;         // 输入文件
    rtpCtx.outFile = out_file;    // 输出文件

    // RTP encode回调
    struct rtp_payload_t handler_rtp_encode_aac;
    handler_rtp_encode_aac.alloc = rtp_alloc;
    handler_rtp_encode_aac.free = rtp_free;
    handler_rtp_encode_aac.packet = rtp_encode_packet;

    rtpCtx.payloadType = 97;  
    rtpCtx.format = "mpeg4-generic";
    rtpCtx.encoder_aac = rtp_payload_encode_create(rtpCtx.payloadType, rtpCtx.format, 1, 0x32411, &handler_rtp_encode_aac, &rtpCtx);

    // RTP decode回调
    struct rtp_payload_t handler_rtp_decode_aac;
    handler_rtp_decode_aac.alloc = rtp_alloc;
    handler_rtp_decode_aac.free = rtp_free;
    handler_rtp_decode_aac.packet = rtp_decode_packet;
    rtpCtx.decoder_aac = rtp_payload_decode_create(rtpCtx.payloadType, rtpCtx.format, &handler_rtp_decode_aac, &rtpCtx);


    // 处理socket通信
    rtpCtx.addr.sin_family = AF_INET;
    rtpCtx.addr.sin_addr.s_addr = inet_addr(DEST_IP);
    rtpCtx.addr.sin_port = htons(DEST_PORT);
    rtpCtx.fd = socket(AF_INET, SOCK_DGRAM, 0);
#if 0
    int ret = fcntl(rtpCtx.fd , F_GETFL , 0);
    fcntl(rtpCtx.fd , F_SETFL , ret | O_NONBLOCK);
#endif
    rtpCtx.addrSize =sizeof(rtpCtx.addr);
    // connect(rtpCtx.fd, (const sockaddr *)&rtpCtx.addr, len) ;//申请UDP套接字

    aac_frame_t *aac_frame =  (aac_frame_t *)malloc(sizeof(aac_frame_t));
    if(!aac_frame)
    {
        printf("malloc aac_frame failed\n");
        return -1;
    }
    memset(aac_frame, 0, sizeof(aac_frame_t));
    if(aac_get_one_frame(aac_frame, bits) < 0)  // 读取一帧aac
    {
        printf("aac_get_one_frame failed\n");
        return -1;
    }

    rtpCtx.profile = aac_frame->header.profile;
    rtpCtx.channel_configuration = aac_frame->header.channel_configuration;
    rtpCtx.sampling_frequency_index = aac_frame->header.sampling_frequency_index;

    aac_rtp_create_sdp("aac.sdp",
                       DEST_IP, DEST_PORT,
                       aac_frame->header.profile,
                       aac_frame->header.channel_configuration,
                       aac_freq[aac_frame->header.sampling_frequency_index],   // 设置的采样率为 timestamp刻度
                       rtpCtx.payloadType);    // 自定义

    int64_t start_time = get_current_time();
    int64_t cur_time = get_current_time();
    double sum_time = 0;                        // 累加总共可以播放的时长
    double frame_duration = 1024*1000/44100;    //一帧数据播放的时长
    int64_t frame_samples = (aac_frame->header.adts_buffer_fullness + 1)/2;

    frame_duration = frame_samples*1000.0/(aac_freq[aac_frame->header.sampling_frequency_index]); // 读取aac信息修正帧持续时间

    uint32_t total_size = 0;
    uint32_t timestamp;//时间戳,us,自增

    while(!feof(bits))
    {
        // 传入的aac帧是带adts header的
        int ret = rtp_payload_encode_input(rtpCtx.encoder_aac, aac_frame->frame_buf, aac_frame->frame_len, timestamp);
        timestamp += (aac_frame->header.adts_buffer_fullness + 1)/2;        // 叠加采样点，单位为1/采样率
//        if(total_size > 200*1024)
//            break;
        total_size += aac_frame->frame_len;
        printf("ret:%d,frame_len:%d,total_size:%uk, frame_duration:%lf\n", ret, aac_frame->frame_len, total_size/1024, frame_duration);
        sum_time += frame_duration;     // 叠加可以播放的时长
        cur_time = get_current_time(); // darren修正发送aac的时间间隔
        while((cur_time - start_time) < (int64_t)(sum_time - 50))
        {
            //            printf("cur_time - start_time:%ld\n", cur_time - start_time);
            //            printf("sum_time:%lf\n",  sum_time);
            usleep(10000);

            cur_time = get_current_time();
            if(feof(bits))
                break;
        }
        if(aac_get_one_frame(aac_frame, bits) < 0)
        {
            printf("aac_get_one_frame failed\n");
            break;
        }
    }

    printf("close file\n");
    fclose(rtpCtx.inFile);
    fclose(rtpCtx.outFile);
}
