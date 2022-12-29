#include "h264-util.h"
#include <stdint.h>

nalu_t *alloc_nalu(int buffersize)
{
    nalu_t *n;

    if ((n = (nalu_t*)calloc(1, sizeof (nalu_t))) == NULL)
    {
        printf("alloc_nalu: n");
        exit(0);
    }

    n->max_size = buffersize;

    if ((n->buf = (char*)calloc(buffersize, sizeof (char))) == NULL)
    {
        free (n);
        printf ("alloc_nalu: n->buf");
        return NULL;
    }

    return n;
}

void free_nalu(nalu_t *n)
{
    if (n)
    {
        if (n->buf)
        {
            free(n->buf);
            n->buf = NULL;
        }
        free (n);
    }
}

FILE *open_bitstream_file (char *filename)
{
    FILE *bits = NULL;
    if (NULL == (bits = fopen(filename, "rb")))
    {
        printf("open file error\n");
    }
    return bits;
}


int get_annexb_nalu (nalu_t *nalu, FILE *bits)
{
    int isStartCode3 = 0;
    int isStartCode4 = 0;
    int pos = 0;
    int nextStartCodeFound = 0;
    int rewind = 0;
    unsigned char *tmpBuf = NULL;

    if ((tmpBuf = (unsigned char*)calloc (nalu->max_size , sizeof(char))) == NULL)
    {
        printf ("get_annexb_nalu: Could not allocate tmpBuf memory\n");
    }

    nalu->startCodeLen = 3;//初始化码流序列的起始码为3个字节

    if (3 != fread(tmpBuf, 1, 3, bits)) //从码流中读3个字节
    {
        free(tmpBuf);
        return 0;
    }
    isStartCode3 = find_start_code_3(tmpBuf); //判断是否为0x000001
    if(isStartCode3 != 1)
    {
        //如果不是3字节的起始码，再读一个字节
        if(1 != fread(tmpBuf+3, 1, 1, bits)) //读一个字节
        {
            free(tmpBuf);
            return 0;
        }
        isStartCode4 = find_start_code_4 (tmpBuf);//判断是否为0x00000001
        if (isStartCode4 != 1)//如果不是，返回-1
        {
            free(tmpBuf);
            return -1;
        }
        else
        {
            //如果是0x00000001,则起始码为4个字节
            pos = 4;
            nalu->startCodeLen = 4;
        }
    }
    else
    {
        //如果是0x000001,则起始码为3个字节
        pos = 3;
        nalu->startCodeLen = 3;
    }

    //查找下一个起始码（是在下面的while循环中不停地找）
    isStartCode3 = 0;
    isStartCode4 = 0;
    
    while (!nextStartCodeFound)
    {
        if (feof (bits))//判断是否到了文件尾，文件结束，则返回非0值，否则返回0
        {
            nalu->len = (pos - 1);  //NALU单元的长度。
            memcpy (nalu->buf, tmpBuf, nalu->len);
            nalu->forbidden_bit = nalu->buf[nalu->startCodeLen] & 0x80;     // 1 bit
            nalu->nal_reference_idc = nalu->buf[nalu->startCodeLen] & 0x60; // 2 bit
            nalu->nal_unit_type = (nalu->buf[nalu->startCodeLen]) & 0x1f;   // 5 bit
            free(tmpBuf);
            return pos-1;
        }

        tmpBuf[pos++] = fgetc(bits); //读一个字节到buf中
        isStartCode4 = find_start_code_4(&tmpBuf[pos-4]); //判断是否为0x00000001
        if(isStartCode4 != 1)
        {
            isStartCode3 = find_start_code_3(&tmpBuf[pos-3]); //判断是否为0x000001
        }

        nextStartCodeFound = (isStartCode3 == 1 || isStartCode4 == 1);
    }

    // Here, we have found another start code (and read length of startcode bytes more than we should have.)
    // Hence, go back in the file
    rewind = (isStartCode4 == 1)? -4 : -3;

    if (0 != fseek (bits, rewind, SEEK_CUR))//把文件指针指向前一个NALU的末尾，在当前文件指针位置上偏移 rewind。
    {
        free(tmpBuf);
        printf("get_annexb_nalu: Cannot fseek in the bit stream file");
    }

    // Here the Start code, the complete NALU, and the next start code is in the buf.
    // The size of buf is pos, pos+rewind are the number of bytes excluding the next
    // start code, and (pos+rewind)-startCodeLen is the size of the NALU excluding the start code
    nalu->len = (pos + rewind);    //NALU长度，不包括下一个起始码。
    memcpy (nalu->buf, tmpBuf, nalu->len); //拷贝一个完整NALU，不包含下一个起始码

    nalu->forbidden_bit = nalu->buf[nalu->startCodeLen] & 0x80;     // 1 bit
    nalu->nal_reference_idc = nalu->buf[nalu->startCodeLen] & 0x60; // 2 bit
    nalu->nal_unit_type = (nalu->buf[nalu->startCodeLen]) & 0x1f;   // 5 bit
    free(tmpBuf);

    return (pos + rewind); //返回两个起始码之间的字节数，即包含有起始码的NALU的长度
}

static int find_start_code_3 (unsigned char *buf)
{
    if(buf[0]!=0 || buf[1]!=0 || buf[2] !=1) return 0; //判断是否为0x000001,如果是返回1
    else return 1;
}

static int find_start_code_4 (unsigned char *buf)
{
    if(buf[0]!=0 || buf[1]!=0 || buf[2] !=0 || buf[3] !=1) return 0;//判断是否为0x00000001,如果是返回1
    else return 1;
}

#define AV_BASE64_SIZE(x)  (((x)+2) / 3 * 4 + 1)
char *av_base64_encode(char *out, int out_size, const unsigned char *in, int in_size)
{
    static const char b64[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    char *ret, *dst;
    unsigned i_bits = 0;
    int i_shift = 0;
    int bytes_remaining = in_size;

    if (in_size >= 0x7fffffff / 4 || out_size < AV_BASE64_SIZE(in_size))
    {
        return NULL;
    }
    ret = dst = out;
    while (bytes_remaining)
    {
        i_bits = (i_bits << 8) + *in++;
        bytes_remaining--;
        i_shift += 8;

        do {
            *dst++ = b64[(i_bits << 6 >> i_shift) & 0x3f];
            i_shift -= 6;
        } while (i_shift > 6 || (bytes_remaining == 0 && i_shift > 0));
    }
    while ((dst - ret) & 3)
        *dst++ = '=';
    *dst = '\0';

    return ret;
}

// 扩展阅读：《用实例分析H264 RTP payload》 https://www.cnblogs.com/shakin/p/3914988.html
void h264_sdp_create(uint8_t *file, uint8_t *ip, uint16_t port,
                     const uint8_t *sps, const int sps_len,
                     const uint8_t *pps, const int pps_len,
                     int payload_type,
                     int time_base,
                     int bitrate)
{
    char buff[1024] = {0};

    char str_profile_level_id[100];
    uint32_t profile_level_id = 0;
    if (sps_len >= 4) { // sanity check
        profile_level_id = sps[1] << 16;
        profile_level_id |= sps[2] << 8;
        profile_level_id |= sps[3];    // profile_idc|constraint_setN_flag|level_idc
    }
    memset(str_profile_level_id, 0, 100);
    sprintf(str_profile_level_id, "%06X", profile_level_id);

    char str_sps[100];
    memset(str_sps, 0, 100);
    av_base64_encode(str_sps, 100, (uint8_t *) sps, sps_len);

    char str_pps[100];
    memset(str_pps, 0, 100);
    av_base64_encode(str_pps, 100, (uint8_t *) pps, pps_len); // 生成SDP的PPS、SPS的写法

    char demo[] =
            "m=video %d RTP/AVP %d\n"
            "a=rtpmap:%d H264/%d\n"
            "a=fmtp:%d profile-level-id=%06X; packetization-mode=1; sprop-parameter-sets=%s,%s\n"
            "c=IN IP4 %s";

    snprintf(buff, sizeof(buff), demo, port, payload_type,
             payload_type, time_base,
             payload_type, profile_level_id, str_sps, str_pps,
             ip);

    printf("h264 sdp:\n%s\n\n", buff);
    remove(file);
    FILE *fd = NULL;
    if((fd = fopen(file, "wt")) > 0)
    {
        fwrite(buff, strlen(buff), 1, fd);
        fclose(fd);
    }
}
