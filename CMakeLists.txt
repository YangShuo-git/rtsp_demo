cmake_minimum_required(VERSION 2.6)

project(rtp)

ADD_DEFINITIONS( -g -W -Wall -D_REENTRANT -D_FILE_OFFSET_BITS=64 -DAC_HAS_INFO
-DAC_HAS_WARNING -DAC_HAS_ERROR -DAC_HAS_CRITICAL -DTIXML_USE_STL
-DAC_HAS_DEBUG -DLINUX_DAEMON)

aux_source_directory(. DIR_SRCS)
     
add_executable(rtp_demo ${DIR_SRCS})








