#ifndef _h264_util_h_
#define _h264_util_h_

#include <stdio.h>
#include <stdint.h>

typedef struct _nalu_t
{
    int startCodeLen;             // 4 for parameter sets and first slice in picture, 3 for everything else (suggested)
    unsigned short lost_packets;  // true, if packet loss is detected
    unsigned max_size;            // Nal Unit Buffer size
    
    unsigned len;                 // Length of the NAL unit (include the start code, which does not belong to the NALU)
    char *buf;                    // include start code


    int forbidden_bit;            // should be always FALSE
    int nal_reference_idc;        // NALU_PRIORITY_xxxx
    int nal_unit_type;            // NALU_TYPE_xxxx
}nalu_t;


// 打开文件
FILE *open_bitstream_file (char *filename);
// 分配nalu
nalu_t *alloc_nalu(int buffersize);
// 释放nalu
void free_nalu(nalu_t *n);

void h264_sdp_create(uint8_t *file, uint8_t *ip, uint16_t port,
                     const uint8_t *sps, const int sps_len,
                     const uint8_t *pps, const int pps_len,
                     int payload_type,
                     int time_base,
                     int bitrate);

// 这个函数输入为一个NAL结构体，主要功能为得到一个完整的NALU并保存在nalu_t的buf中，获取长度，并填充F,IDC,TYPE位。
// 并且返回两个开始字符之间间隔的字节数，即包含有起始码的NALU的长度
int get_annexb_nalu (nalu_t *nalu, FILE *bits);

// 查找起始码0x000001
static int find_start_code_3 (unsigned char *buf); 

// 查找起始码x00000001
static int find_start_code_4 (unsigned char *buf); 

#endif // H264UTIL_H
