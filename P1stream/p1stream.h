#ifndef p1stream_h
#define p1stream_h

#include <stdbool.h>
#include <stdint.h>
#include <x264.h>
#include <IOSurface/IOSurface.h>


typedef struct _P1Config P1Config;
typedef struct _P1ConfigSection P1ConfigSection; // opaque

typedef bool (*P1ConfigIterSection)(P1Config *cfg, P1ConfigSection *sect, void *data);
typedef bool (*P1ConfigIterString)(P1Config *cfg, const char *key, char *val, void *data);

struct _P1Config {
    void (*free)(P1Config *cfg);
    P1ConfigSection *(*get_section)(P1Config *cfg, P1ConfigSection *sect, const char *key);
    bool (*get_string)(P1Config *cfg, P1ConfigSection *sect, const char *key, char *buf, size_t bufsize);

    bool (*each_section)(P1Config *cfg, P1ConfigSection *sect, const char *key, P1ConfigIterSection iter, void *data);
    bool (*each_string)(P1Config *cfg, P1ConfigSection *sect, const char *key, P1ConfigIterString iter, void *data);
};


typedef struct _P1VideoClock P1VideoClock;
typedef struct _P1VideoClockFactory P1VideoClockFactory;

struct _P1VideoClock {
    P1VideoClockFactory *factory;
    void (*free)(P1VideoClock *clock);

    bool (*start)(P1VideoClock *clock);
    void (*stop)(P1VideoClock *clock);
};

struct _P1VideoClockFactory {
    P1VideoClock *(*create)();
};


typedef struct _P1VideoSource P1VideoSource;
typedef struct _P1VideoSourceFactory P1VideoSourceFactory;

struct _P1VideoSource {
    P1VideoSourceFactory *factory;
    void (*free)(P1VideoSource *source);

    bool (*start)(P1VideoSource *source);
    void (*frame)(P1VideoSource *source);
    void (*stop)(P1VideoSource *source);
};

struct _P1VideoSourceFactory {
    P1VideoSource *(*create)();
};


typedef struct _P1AudioSource P1AudioSource;
typedef struct _P1AudioSourceFactory P1AudioSourceFactory;

struct _P1AudioSource {
    P1AudioSourceFactory *factory;
    void (*free)(P1AudioSource *src);

    bool (*start)(P1AudioSource *src);
    void (*stop)(P1AudioSource *src);
};

struct _P1AudioSourceFactory {
    P1AudioSource *(*create)();
};


// FIXME: combine these, remove globals, single state struct.
void p1_video_init(P1Config *cfg);
void p1_audio_init();
void p1_stream_init(P1Config *cfg);

void p1_video_set_clock(P1VideoClock *src);
void p1_video_add_source(P1VideoSource *src);

void p1_video_clock_tick(P1VideoClock *src, int64_t time);
void p1_video_frame_raw(P1VideoSource *src, int width, int height, void *data);
void p1_video_frame_iosurface(P1VideoSource *src, IOSurfaceRef buffer);

void p1_audio_add_source(P1AudioSource *src);
void p1_audio_mix(P1AudioSource *dtv, int64_t time, void *in, int in_len);

// FIXME: These should be internal.
void p1_stream_video_config(x264_nal_t *nals, int len);
void p1_stream_video(x264_nal_t *nals, int len, x264_picture_t *pic);

void p1_stream_audio_config();
void p1_stream_audio(int64_t time, void *buf, int len);

#endif
