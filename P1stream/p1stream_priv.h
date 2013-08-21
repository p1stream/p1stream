#ifndef p1stream_priv_h
#define p1stream_priv_h

#include <mach/mach_time.h>
#include <dispatch/dispatch.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>
#include <OpenCL/opencl.h>
#include <aacenc_lib.h>
#include <x264.h>
#include <rtmp.h>

#include "p1stream.h"


#define P1_PACKET_QUEUE_LENGTH 256

struct _P1Context {
    mach_timebase_info_data_t timebase;


    // Audio
    P1AudioSource *audio_src;

    HANDLE_AACENCODER aac;
    void *mix;
    int mix_len;
    void *out;

    int64_t time;

    bool sent_audio_config;


    // Video
    P1VideoClock *video_clock;
    P1VideoSource *video_src;

    size_t skip_counter;

    CGLContextObj gl;

    cl_context cl;
    cl_command_queue clq;

    GLuint vao;
    GLuint vbo;
    GLuint rbo;
    GLuint fbo;
    GLuint tex;
    GLuint program;
    GLuint tex_u;

    cl_mem rbo_mem;
    cl_mem out_mem;
    cl_kernel yuv_kernel;

    x264_t *enc;
    x264_picture_t enc_pic;

    bool sent_video_config;


    // Stream
    dispatch_queue_t dispatch;

    RTMP rtmp;
    char url[256];

    // ring buffer
    RTMPPacket *q[P1_PACKET_QUEUE_LENGTH];
    size_t q_start;
    size_t q_len;

    uint64_t start;
};

void p1_audio_init(P1Context *ctx, P1Config *cfg);
void p1_video_init(P1Context *ctx, P1Config *cfg);
void p1_stream_init(P1Context *ctx, P1Config *cfg);

void p1_stream_video_config(P1Context *ctx, x264_nal_t *nals, int len);
void p1_stream_video(P1Context *ctx, x264_nal_t *nals, int len, x264_picture_t *pic);

void p1_stream_audio_config(P1Context *ctx);
void p1_stream_audio(P1Context *ctx, int64_t time, void *buf, int len);

#endif
