#ifndef p1_cli_video_h
#define p1_cli_video_h

#include <stdint.h>
#include <IOSurface/IOSurface.h>

typedef struct _P1VideoPlugin P1VideoPlugin;
typedef struct _P1VideoSource P1VideoSource;

struct _P1VideoPlugin {
    P1VideoSource *(*create)();
    void (*free)(P1VideoSource *src);

    int (*start)(P1VideoSource *src);
    void (*stop)(P1VideoSource *src);
};

struct _P1VideoSource {
    P1VideoPlugin *plugin;
};

void p1_video_init();
void p1_video_add_source(P1VideoSource *src);
void p1_video_frame_idle(P1VideoSource *src, int64_t time);
void p1_video_frame_blank(P1VideoSource *src, int64_t time);
void p1_video_frame_raw(P1VideoSource *src, int64_t time, int width, int height, void *data);
void p1_video_frame_iosurface(P1VideoSource *src, int64_t time, IOSurfaceRef buffer);

#endif
