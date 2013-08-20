#ifndef p1_cli_video_h
#define p1_cli_video_h

#include <stdbool.h>
#include <stdint.h>
#include <IOSurface/IOSurface.h>

#include "conf.h"


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


void p1_video_init(P1Config *cfg);

void p1_video_set_clock(P1VideoClock *src);
void p1_video_add_source(P1VideoSource *src);

void p1_video_clock_tick(P1VideoClock *src, int64_t time);
void p1_video_frame_raw(P1VideoSource *src, int width, int height, void *data);
void p1_video_frame_iosurface(P1VideoSource *src, IOSurfaceRef buffer);

#endif
