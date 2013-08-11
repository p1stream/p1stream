#ifndef p1_cli_stream_h
#define p1_cli_stream_h

#include <x264.h>
#include <AudioToolbox/AudioToolbox.h>

void p1_stream_init(const char *url);
void p1_stream_video_config(x264_nal_t *nals, int len);
void p1_stream_video(x264_nal_t *nals, int len, x264_picture_t *pic);
void p1_stream_audio_config();
void p1_stream_audio(AudioQueueBufferRef buf, uint64_t time);

#endif
