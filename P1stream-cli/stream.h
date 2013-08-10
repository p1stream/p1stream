#ifndef P1stream_cli_stream_h
#define P1stream_cli_stream_h

#include <x264.h>

void p1_stream_init(const char *url);
void p1_stream_video_config(x264_nal_t *nals, int len);
void p1_stream_video(x264_nal_t *nals, int len, x264_picture_t *pic);

#endif
