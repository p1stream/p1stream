// Video source that captures the display using CGDisplayStream (10.8+).

#include <stdio.h>
#include <pthread.h>
#include <dispatch/dispatch.h>
#include <CoreGraphics/CoreGraphics.h>

#include "p1stream.h"


typedef struct _P1DisplayVideoSource P1DisplayVideoSource;

struct _P1DisplayVideoSource {
    P1VideoSource super;

    dispatch_queue_t dispatch;

    IOSurfaceRef frame;
    pthread_mutex_t frame_lock;

    CGDisplayStreamRef display_stream;
};

static void p1_display_video_source_free(P1Source *src);
static bool p1_display_video_source_start(P1Source *src);
static void p1_display_video_source_stop(P1Source *src);
static void p1_display_video_source_frame(P1VideoSource *vsrc);
static void p1_display_video_source_callback(
    P1DisplayVideoSource *dvsrc,
    CGDisplayStreamFrameStatus status,
    IOSurfaceRef frame);


P1VideoSource *p1_display_video_source_create()
{
    P1DisplayVideoSource *dvsrc = calloc(1, sizeof(P1DisplayVideoSource));
    P1VideoSource *vsrc = (P1VideoSource *) dvsrc;
    P1Source *src = (P1Source *) dvsrc;
    assert(dvsrc != NULL);

    src->free = p1_display_video_source_free;
    src->start = p1_display_video_source_start;
    src->stop = p1_display_video_source_stop;
    vsrc->frame = p1_display_video_source_frame;

    dvsrc->dispatch = dispatch_queue_create("video_desktop", DISPATCH_QUEUE_SERIAL);

    pthread_mutex_init(&dvsrc->frame_lock, NULL);

    const CGDirectDisplayID display_id = kCGDirectMainDisplay;
    size_t width  = CGDisplayPixelsWide(display_id);
    size_t height = CGDisplayPixelsHigh(display_id);

    dvsrc->display_stream = CGDisplayStreamCreateWithDispatchQueue(
        display_id, width, height, 'BGRA', NULL, dvsrc->dispatch, ^(
            CGDisplayStreamFrameStatus status,
            uint64_t displayTime,
            IOSurfaceRef frameSurface,
            CGDisplayStreamUpdateRef updateRef)
        {
            p1_display_video_source_callback(dvsrc, status, frameSurface);
        });
    assert(dvsrc->display_stream);

    return vsrc;
}

static void p1_display_video_source_free(P1Source *src)
{
    P1DisplayVideoSource *dvsrc = (P1DisplayVideoSource *)src;

    CFRelease(dvsrc->display_stream);
    dispatch_release(dvsrc->dispatch);

    pthread_mutex_destroy(&dvsrc->frame_lock);
}

static bool p1_display_video_source_start(P1Source *src)
{
    P1DisplayVideoSource *dvsrc = (P1DisplayVideoSource *)src;

    CGError cg_ret = CGDisplayStreamStart(dvsrc->display_stream);
    assert(cg_ret == kCGErrorSuccess);

    // FIXME: Should we wait for anything?
    src->state = P1StateRunning;

    return true;
}

static void p1_display_video_source_stop(P1Source *src)
{
    P1DisplayVideoSource *dvsrc = (P1DisplayVideoSource *)src;

    CGError cg_ret = CGDisplayStreamStop(dvsrc->display_stream);
    assert(cg_ret == kCGErrorSuccess);

    // FIXME: Should we wait for anything?
    src->state = P1StateIdle;
}

static void p1_display_video_source_frame(P1VideoSource *vsrc)
{
    P1DisplayVideoSource *dvsrc = (P1DisplayVideoSource *)vsrc;
    IOSurfaceRef frame;

    pthread_mutex_lock(&dvsrc->frame_lock);
    frame = dvsrc->frame;
    if (frame) {
        CFRetain(frame);
        IOSurfaceIncrementUseCount(frame);
    }
    pthread_mutex_unlock(&dvsrc->frame_lock);

    if (!frame)
        return;

    p1_video_frame_iosurface(vsrc, frame);

    IOSurfaceDecrementUseCount(frame);
    CFRelease(frame);
}

static void p1_display_video_source_callback(
    P1DisplayVideoSource *source,
    CGDisplayStreamFrameStatus status,
    IOSurfaceRef frame)
{
    if (status == kCGDisplayStreamFrameStatusFrameComplete) {
        CFRetain(frame);
        IOSurfaceIncrementUseCount(frame);
    }

    IOSurfaceRef old_frame = NULL;

    pthread_mutex_lock(&source->frame_lock);
    if (status != kCGDisplayStreamFrameStatusFrameIdle) {
        old_frame = source->frame;
        source->frame = NULL;
    }
    if (status == kCGDisplayStreamFrameStatusFrameComplete) {
        source->frame = frame;
    }
    pthread_mutex_unlock(&source->frame_lock);

    if (old_frame) {
        IOSurfaceDecrementUseCount(old_frame);
        CFRelease(old_frame);
    }

    if (status == kCGDisplayStreamFrameStatusStopped) {
        printf("Display stream stopped.");
        abort();
    }
}
