#ifndef p1_cli_stream_h
#define p1_cli_stream_h

#include <stdint.h>
#include <x264.h>
#include <AudioToolbox/AudioToolbox.h>

#include "conf.h"

void p1_stream_init(P1Config *cfg);
void p1_stream_video_config(x264_nal_t *nals, int len);
void p1_stream_video(x264_nal_t *nals, int len, x264_picture_t *pic);
void p1_stream_audio_config();
void p1_stream_audio(int64_t time, void *buf, int len);

#endif
