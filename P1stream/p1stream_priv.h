#ifndef p1stream_priv_h
#define p1stream_priv_h

#include "p1stream.h"

#include <mach/mach_time.h>
#include <OpenCL/opencl.h>
#include <aacenc_lib.h>
#include <x264.h>
#include <rtmp.h>

typedef struct _P1PacketQueue P1PacketQueue;
typedef struct _P1VideoFull P1VideoFull;
typedef struct _P1AudioFull P1AudioFull;
typedef struct _P1ConnectionFull P1ConnectionFull;
typedef struct _P1ContextFull P1ContextFull;


// Private P1Object methods.
void p1_object_init(P1Object *obj);
void p1_object_destroy(P1Object *obj);


// This is a ringbuffer of RMTPPacket pointers.

struct _P1PacketQueue {
    RTMPPacket *head[UINT8_MAX + 1];
    uint8_t read;
    uint8_t write;
    uint8_t length;
};


// Private part of P1Video.

struct _P1VideoFull {
    P1Video super;

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

    bool sent_config;
};

void p1_video_init(P1VideoFull *videof, P1Config *cfg, P1ConfigSection *sect);
void p1_video_start(P1VideoFull *videof);
void p1_video_stop(P1VideoFull *videof);


// Private part of P1Audio.

struct _P1AudioFull {
    P1Audio super;

    HANDLE_AACENCODER aac;
    float *mix;
    INT_PCM *enc_in;
    void *out;

    int64_t time;

    bool sent_config;
};

void p1_audio_init(P1AudioFull *audiof, P1Config *cfg, P1ConfigSection *sect);
#define p1_audio_destroy(_audiof) p1_object_destroy((P1Object *) _audiof)
void p1_audio_start(P1AudioFull *audiof);
void p1_audio_stop(P1AudioFull *audiof);


// Private part of P1StreamConnection.

struct _P1ConnectionFull {
    P1Connection super;

    char url[2048];

    uint64_t start;

    pthread_t thread;
    pthread_cond_t cond;
    P1PacketQueue video_queue;
    P1PacketQueue audio_queue;
};

void p1_conn_init(P1ConnectionFull *connf, P1Config *cfg, P1ConfigSection *sect);
void p1_conn_destroy(P1ConnectionFull *connf);
void p1_conn_start(P1ConnectionFull *connf);
void p1_conn_stop(P1ConnectionFull *connf);
void p1_conn_video_config(P1ConnectionFull *connf, x264_nal_t *nals, int len);
void p1_conn_video(P1ConnectionFull *connf, x264_nal_t *nals, int len, x264_picture_t *pic);
void p1_conn_audio_config(P1ConnectionFull *connf);
void p1_conn_audio(P1ConnectionFull *connf, int64_t time, void *buf, size_t len);


// Private part of P1Context.

struct _P1ContextFull {
    P1Context super;

    pthread_t ctrl_thread;
    int ctrl_pipe[2];
    int user_pipe[2];

    mach_timebase_info_data_t timebase;
};

#endif
