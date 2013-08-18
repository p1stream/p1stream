#include <stdio.h>
#include <mach/mach_time.h>
#include <dispatch/dispatch.h>
#include <CoreGraphics/CoreGraphics.h>

#include "video.h"

static const size_t fps = 60;

typedef struct _P1VideoDesktopSource P1VideoDesktopSource;

struct _P1VideoDesktopSource {
    P1VideoSource super;

    dispatch_queue_t dispatch;

    CGDisplayStreamRef display_stream;

    uint64_t last_time;
    uint64_t frame_period;
};

static P1VideoSource *p1_video_desktop_create();
static void p1_video_desktop_free(P1VideoSource *_source);
static bool p1_video_desktop_start(P1VideoSource *_source);
static void p1_video_desktop_stop(P1VideoSource *_source);
static void p1_video_desktop_frame(
    P1VideoDesktopSource *source, CGDisplayStreamFrameStatus status,
    uint64_t displayTime, IOSurfaceRef frameSurface);

P1VideoPlugin p1_video_desktop = {
    .create = p1_video_desktop_create,
    .free = p1_video_desktop_free,

    .start = p1_video_desktop_start,
    .stop = p1_video_desktop_stop
};


static P1VideoSource *p1_video_desktop_create()
{
    P1VideoDesktopSource *source = calloc(1, sizeof(P1VideoDesktopSource));
    assert(source != NULL);

    P1VideoSource *_source = (P1VideoSource *) source;
    _source->plugin = &p1_video_desktop;

    source->dispatch = dispatch_queue_create("video_desktop", DISPATCH_QUEUE_SERIAL);

    const CGDirectDisplayID display_id = kCGDirectMainDisplay;
    size_t width  = CGDisplayPixelsWide(display_id);
    size_t height = CGDisplayPixelsHigh(display_id);

    source->display_stream = CGDisplayStreamCreateWithDispatchQueue(
        display_id, width, height, 'BGRA', NULL, source->dispatch,
        ^(CGDisplayStreamFrameStatus status, uint64_t displayTime, IOSurfaceRef frameSurface, CGDisplayStreamUpdateRef updateRef) {
            p1_video_desktop_frame(source, status, displayTime, frameSurface);
        });
    assert(source->display_stream);

    mach_timebase_info_data_t base;
    mach_timebase_info(&base);
    source->frame_period = 1000000000 / fps * base.denom / base.numer;

    return _source;
}

static void p1_video_desktop_free(P1VideoSource *_source)
{
    P1VideoDesktopSource *source = (P1VideoDesktopSource *)_source;

    CFRelease(source->display_stream);
    dispatch_release(source->dispatch);
}

static bool p1_video_desktop_start(P1VideoSource *_source)
{
    P1VideoDesktopSource *source = (P1VideoDesktopSource *)_source;

    CGError err = CGDisplayStreamStart(source->display_stream);
    assert(err == kCGErrorSuccess);

    return true;
}

static void p1_video_desktop_stop(P1VideoSource *_source)
{
    P1VideoDesktopSource *source = (P1VideoDesktopSource *)_source;

    CGError err = CGDisplayStreamStop(source->display_stream);
    assert(err == kCGErrorSuccess);
}

static void p1_video_desktop_frame(
    P1VideoDesktopSource *source, CGDisplayStreamFrameStatus status,
    uint64_t displayTime, IOSurfaceRef frameSurface)
{
    P1VideoSource *_source = (P1VideoSource *) source;

    // When idle, we get frames at a lower rate, apparently 15hz.
    // Pad with idle frames.
    if (source->last_time == 0) {
        source->last_time = displayTime;
    }
    else {
        source->last_time += source->frame_period;
        while (source->last_time < displayTime) {
            p1_video_frame_idle(_source, source->last_time);
            source->last_time += source->frame_period;
        }
    }

    switch (status) {
        case kCGDisplayStreamFrameStatusFrameComplete:
            p1_video_frame_iosurface(_source, displayTime, frameSurface);
            break;
        case kCGDisplayStreamFrameStatusFrameIdle:
            p1_video_frame_idle(_source, displayTime);
            break;
        case kCGDisplayStreamFrameStatusFrameBlank:
            p1_video_frame_blank(_source, displayTime);
            break;
        case kCGDisplayStreamFrameStatusStopped:
            printf("Display stream stopped.");
            abort();
    }
}
