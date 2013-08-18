// Video source that captures the desktop.
//
// This uses a combination of a CGDisplayStream (10.8+) and a CVDisplayLink.
// The display link is needed because the display stream throttles back when
// there are no updates. In practice, the display link will always call back
// after the display stream.

#include <stdio.h>
#include <pthread.h>
#include <dispatch/dispatch.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreVideo/CoreVideo.h>

#include "video.h"

static const size_t fps = 60;

typedef struct _P1VideoDesktopSource P1VideoDesktopSource;

struct _P1VideoDesktopSource {
    P1VideoSource super;

    dispatch_queue_t dispatch;

    IOSurfaceRef frame;
    pthread_mutex_t frame_lock;

    CGDisplayStreamRef display_stream;
    CVDisplayLinkRef display_link;
};

static P1VideoSource *p1_video_desktop_create();
static void p1_video_desktop_free(P1VideoSource *_source);
static bool p1_video_desktop_start(P1VideoSource *_source);
static void p1_video_desktop_stop(P1VideoSource *_source);
static void p1_video_desktop_frame(
    P1VideoDesktopSource *source,
    CGDisplayStreamFrameStatus status,
    IOSurfaceRef frame);
static CVReturn p1_video_desktop_clock(
    CVDisplayLinkRef displayLink,
    const CVTimeStamp *inNow,
    const CVTimeStamp *inOutputTime,
    CVOptionFlags flagsIn,
    CVOptionFlags *flagsOut,
    void *displayLinkContext);

P1VideoPlugin p1_video_desktop = {
    .create = p1_video_desktop_create,
    .free = p1_video_desktop_free,

    .start = p1_video_desktop_start,
    .stop = p1_video_desktop_stop
};


static P1VideoSource *p1_video_desktop_create()
{
    CVReturn ret;

    P1VideoDesktopSource *source = calloc(1, sizeof(P1VideoDesktopSource));
    assert(source != NULL);

    P1VideoSource *_source = (P1VideoSource *) source;
    _source->plugin = &p1_video_desktop;

    source->dispatch = dispatch_queue_create("video_desktop", DISPATCH_QUEUE_SERIAL);

    pthread_mutex_init(&source->frame_lock, NULL);

    const CGDirectDisplayID display_id = kCGDirectMainDisplay;
    size_t width  = CGDisplayPixelsWide(display_id);
    size_t height = CGDisplayPixelsHigh(display_id);

    source->display_stream = CGDisplayStreamCreateWithDispatchQueue(
        display_id, width, height, 'BGRA', NULL, source->dispatch, ^(
            CGDisplayStreamFrameStatus status,
            uint64_t displayTime,
            IOSurfaceRef frameSurface,
            CGDisplayStreamUpdateRef updateRef)
        {
            p1_video_desktop_frame(source, status, frameSurface);
        });
    assert(source->display_stream);

    ret = CVDisplayLinkCreateWithCGDisplay(display_id, &source->display_link);
    assert(ret == kCVReturnSuccess);
    ret = CVDisplayLinkSetOutputCallback(source->display_link, p1_video_desktop_clock, source);
    assert(ret == kCVReturnSuccess);

    return _source;
}

static void p1_video_desktop_free(P1VideoSource *_source)
{
    P1VideoDesktopSource *source = (P1VideoDesktopSource *)_source;

    CFRelease(source->display_stream);
    dispatch_release(source->dispatch);

    pthread_mutex_destroy(&source->frame_lock);
}

static bool p1_video_desktop_start(P1VideoSource *_source)
{
    P1VideoDesktopSource *source = (P1VideoDesktopSource *)_source;

    CGError cg_ret = CGDisplayStreamStart(source->display_stream);
    assert(cg_ret == kCGErrorSuccess);

    CVReturn cv_ret = CVDisplayLinkStart(source->display_link);
    assert(cv_ret == kCVReturnSuccess);

    return true;
}

static void p1_video_desktop_stop(P1VideoSource *_source)
{
    P1VideoDesktopSource *source = (P1VideoDesktopSource *)_source;

    CVReturn cv_ret = CVDisplayLinkStop(source->display_link);
    assert(cv_ret == kCVReturnSuccess);

    CGError cg_ret = CGDisplayStreamStop(source->display_stream);
    assert(cg_ret == kCGErrorSuccess);
}

static void p1_video_desktop_frame(
    P1VideoDesktopSource *source,
    CGDisplayStreamFrameStatus status,
    IOSurfaceRef frame)
{
    pthread_mutex_lock(&source->frame_lock);
    if (status != kCGDisplayStreamFrameStatusFrameIdle) {
        if (source->frame) {
            IOSurfaceDecrementUseCount(source->frame);
            CFRelease(source->frame);
            source->frame = NULL;
        }
    }
    if (status == kCGDisplayStreamFrameStatusFrameComplete) {
        CFRetain(frame);
        IOSurfaceIncrementUseCount(frame);
        source->frame = frame;
    }
    pthread_mutex_unlock(&source->frame_lock);

    if (status == kCGDisplayStreamFrameStatusStopped) {
        printf("Display stream stopped.");
        abort();
    }
}

static CVReturn p1_video_desktop_clock(
    CVDisplayLinkRef displayLink,
    const CVTimeStamp *inNow,
    const CVTimeStamp *inOutputTime,
    CVOptionFlags flagsIn,
    CVOptionFlags *flagsOut,
    void *displayLinkContext)
{
    P1VideoDesktopSource *source = (P1VideoDesktopSource *)displayLinkContext;
    P1VideoSource *_source = (P1VideoSource *)source;

    IOSurfaceRef frame = NULL;

    pthread_mutex_lock(&source->frame_lock);
    frame = source->frame;
    if (frame) {
        CFRetain(frame);
        IOSurfaceIncrementUseCount(frame);
    }
    pthread_mutex_unlock(&source->frame_lock);

    p1_video_frame_iosurface(_source, inNow->hostTime, frame);

    IOSurfaceDecrementUseCount(frame);
    CFRelease(frame);

    return kCVReturnSuccess;
}
