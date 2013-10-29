#ifndef p1stream_priv_h
#define p1stream_priv_h

#include "p1stream.h"

#include <aacenc_lib.h>
#include <x264.h>
#include <librtmp/rtmp.h>
#include <librtmp/log.h>

typedef struct _P1PacketQueue P1PacketQueue;
typedef struct _P1VideoFull P1VideoFull;
typedef struct _P1AudioFull P1AudioFull;
typedef struct _P1ConnectionFull P1ConnectionFull;
typedef struct _P1ContextFull P1ContextFull;


// Platform-specific support.

#if __APPLE__
#   include <TargetConditionals.h>
#   if TARGET_OS_MAC
#       include "osx/p1stream_osx_priv.h"
#   else
#       error Unsupported platform
#   endif
#else
#   error Unsupported platform
#endif


// The checks performed on flags to see if an object can start.

#define P1_FLAGS_STARTABLE_MASK     (P1_FLAG_CONFIG_VALID | P1_FLAG_CAN_START | P1_FLAG_ERROR)
#define P1_FLAGS_STARTABLE_VALUE    (P1_FLAG_CONFIG_VALID | P1_FLAG_CAN_START)

#define p1_startable_flags(_flags)                  \
    (((_flags) & P1_FLAGS_STARTABLE_MASK) == P1_FLAGS_STARTABLE_VALUE)

// The flags that are reset before the config method is called.

#define P1_FLAGS_CONFIG_MASK        (P1_FLAG_CONFIG_VALID | P1_FLAG_NEEDS_RESTART)
#define P1_FLAGS_CONFIG_VALUE       (P1_FLAG_CONFIG_VALID)

#define p1_object_reset_config_flags(_obj) ({       \
    P1Flags *_p1_flags = &(_obj)->state.flags;      \
    *_p1_flags = ((*_p1_flags) & ~P1_FLAGS_CONFIG_MASK) | P1_FLAGS_CONFIG_VALUE;    \
})

// The flags that are reset before the notify method is called.

#define P1_FLAGS_NOTIFY_MASK        (P1_FLAG_CAN_START)
#define P1_FLAGS_NOTIFY_VALUE       (P1_FLAG_CAN_START)

#define p1_object_reset_notify_flags(_obj) ({       \
    P1Flags *_p1_flags = &(_obj)->state.flags;      \
    *_p1_flags = ((*_p1_flags) & ~P1_FLAGS_NOTIFY_MASK) | P1_FLAGS_NOTIFY_VALUE;    \
})


// Private P1Object methods.

bool p1_object_init(P1Object *obj, P1ObjectType type, P1Context *ctx);
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

    // These are initialized by platform support
    P1GLContext gl;
    cl_context cl;
    GLuint tex;
    GLuint fbo;

    // Config
    int cfg_width;
    int cfg_height;

    // GL objects
    GLuint vao;
    GLuint vbo;
    GLuint program;
    GLuint tex_u;

    // CL objects
    size_t out_size;
    size_t yuv_work_size[2];
    cl_command_queue clq;
    cl_mem tex_mem;
    cl_mem out_mem;
    cl_kernel yuv_kernel;

    // Output
    x264_picture_t out_pic;
};

bool p1_video_init(P1VideoFull *videof, P1Context *ctx);
#define p1_video_destroy(_videof) p1_object_destroy((P1Object *) _videof)

void p1_video_config(P1VideoFull *videof, P1Config *cfg);
void p1_video_notify(P1VideoFull *videof, P1Notification *n);

void p1_video_start(P1VideoFull *videof);
void p1_video_stop(P1VideoFull *videof);

void p1_video_cl_notify_callback(const char *errstr, const void *private_info, size_t cb, void *user_data);

void p1_video_clock_notify(P1VideoClock *vclock, P1Notification *n);
void p1_video_source_notify(P1VideoSource *vsrc, P1Notification *n);


// Private part of P1Audio.

struct _P1AudioFull {
    P1Audio super;

    // Mix buffer
    float *mix;
    int64_t mix_time;

    // Output buffer
    int16_t *out;
    size_t out_pos;
    int64_t out_time;

    // Mix thread
    pthread_t thread;
    pthread_cond_t cond;
};

bool p1_audio_init(P1AudioFull *audiof, P1Context *ctx);
void p1_audio_destroy(P1AudioFull *audiof);

void p1_audio_config(P1AudioFull *audiof, P1Config *cfg);
void p1_audio_notify(P1AudioFull *audiof, P1Notification *n);

void p1_audio_start(P1AudioFull *audiof);
void p1_audio_stop(P1AudioFull *audiof);

void p1_audio_source_notify(P1AudioSource *asrc, P1Notification *n);


// Private part of P1StreamConnection.

struct _P1ConnectionFull {
    P1Connection super;

    // Config
    char cfg_url[2048];
    x264_param_t cfg_video_params;

    // RTMP state
    char url[2048];
    RTMP rtmp;

    // Start time, used as the zero point for RTMP timestamps
    uint64_t start;

    // Connection thread
    pthread_t thread;
    pthread_cond_t cond;

    // Packet queue
    P1PacketQueue video_queue;
    P1PacketQueue audio_queue;

    // Video encoding
    pthread_mutex_t video_lock;
    x264_param_t video_params;
    float keyint_sec;
    x264_t *video_enc;

    // Audio encoding
    pthread_mutex_t audio_lock;
    HANDLE_AACENCODER audio_enc;
    void *audio_out;
};

bool p1_conn_init(P1ConnectionFull *connf, P1Context *ctx);
void p1_conn_destroy(P1ConnectionFull *connf);

void p1_conn_config(P1ConnectionFull *connf, P1Config *cfg);
void p1_conn_notify(P1ConnectionFull *connf, P1Notification *n);

void p1_conn_start(P1ConnectionFull *connf);
void p1_conn_stop(P1ConnectionFull *connf);

void p1_conn_stream_video(P1ConnectionFull *connf, int64_t time, x264_picture_t *pic);
size_t p1_conn_stream_audio(P1ConnectionFull *connf, int64_t time, int16_t *buf, size_t samples);


// Private part of P1Context.

struct _P1ContextFull {
    P1Context super;

    pthread_t ctrl_thread;
    int ctrl_pipe[2];
    int user_pipe[2];

    // The time base of OS timestamps, which are used throughout interface, as
    // a fraction of nanoseconds. The reference date is not important to us.
    uint32_t timebase_num;
    uint32_t timebase_den;
};

#endif
