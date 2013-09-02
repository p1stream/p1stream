#ifndef p1stream_priv_h
#define p1stream_priv_h

#include "p1stream.h"

#include <mach/mach_time.h>
#include <OpenCL/opencl.h>
#include <aacenc_lib.h>
#include <x264.h>
#include <rtmp.h>

typedef struct _P1PacketQueue P1PacketQueue;
typedef struct _P1ContextFull P1ContextFull;


struct _P1PacketQueue {
    RTMPPacket *head[UINT8_MAX + 1];
    uint8_t read;
    uint8_t write;
    uint8_t length;
};

struct _P1ContextFull {
    P1Context super;

    // Control thread
    pthread_t ctrl_thread;
    int ctrl_pipe[2];
    int user_pipe[2];

    mach_timebase_info_data_t timebase;


    // Audio
    bool audio_ready;

    HANDLE_AACENCODER aac;
    float *mix;
    INT_PCM *enc_in;
    void *out;

    int64_t time;

    bool sent_audio_config;


    // Video
    bool video_ready;

    CGLContextObj gl;

    cl_context cl;
    cl_command_queue clq;

    GLuint vao;
    GLuint vbo;
    GLuint rbo;
    GLuint fbo;
    GLuint program;
    GLuint tex_u;

    cl_mem rbo_mem;
    cl_mem out_mem;
    cl_kernel yuv_kernel;

    x264_t *enc;
    x264_param_t params;
    x264_picture_t enc_pic;

    bool sent_video_config;


    // Stream
    P1State stream_state;

    RTMP rtmp;
    char url[256];

    uint64_t start;

    pthread_t stream_thread;
    pthread_mutex_t stream_lock;
    pthread_cond_t stream_cond;
    P1PacketQueue video_queue;
    P1PacketQueue audio_queue;
};


void p1_audio_init(P1ContextFull *ctx, P1Config *cfg, P1ConfigSection *sect);
void p1_video_init(P1ContextFull *ctx, P1Config *cfg, P1ConfigSection *sect);
void p1_stream_init(P1ContextFull *ctx, P1Config *cfg, P1ConfigSection *sect);

void p1_audio_start(P1ContextFull *ctx);
void p1_video_start(P1ContextFull *ctx);
void p1_stream_start(P1ContextFull *ctx);

void p1_video_output(P1VideoClock *vclock, int64_t time);

void p1_stream_video_config(P1ContextFull *ctx, x264_nal_t *nals, int len);
void p1_stream_video(P1ContextFull *ctx, x264_nal_t *nals, int len, x264_picture_t *pic);

void p1_stream_audio_config(P1ContextFull *ctx);
void p1_stream_audio(P1ContextFull *ctx, int64_t time, void *buf, size_t len);

#endif
