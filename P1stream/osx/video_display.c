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

static P1VideoSource *p1_display_video_source_create();
static void p1_display_video_source_free(P1VideoSource *_source);
static bool p1_display_video_source_start(P1VideoSource *_source);
static void p1_display_video_source_frame(P1VideoSource *_source);
static void p1_display_video_source_stop(P1VideoSource *_source);
static void p1_display_video_source_callback(
    P1DisplayVideoSource *source,
    CGDisplayStreamFrameStatus status,
    IOSurfaceRef frame);

P1VideoSourceFactory p1_display_video_source_factory = {
    .create = p1_display_video_source_create
};


static P1VideoSource *p1_display_video_source_create()
{
    P1DisplayVideoSource *source = calloc(1, sizeof(P1DisplayVideoSource));
    assert(source != NULL);

    P1VideoSource *_source = (P1VideoSource *) source;
    _source->factory = &p1_display_video_source_factory;
    _source->free = p1_display_video_source_free;
    _source->start = p1_display_video_source_start;
    _source->frame = p1_display_video_source_frame;
    _source->stop = p1_display_video_source_stop;

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
            p1_display_video_source_callback(source, status, frameSurface);
        });
    assert(source->display_stream);

    return _source;
}

static void p1_display_video_source_free(P1VideoSource *_source)
{
    P1DisplayVideoSource *source = (P1DisplayVideoSource *)_source;

    CFRelease(source->display_stream);
    dispatch_release(source->dispatch);

    pthread_mutex_destroy(&source->frame_lock);
}

static bool p1_display_video_source_start(P1VideoSource *_source)
{
    P1DisplayVideoSource *source = (P1DisplayVideoSource *)_source;

    CGError cg_ret = CGDisplayStreamStart(source->display_stream);
    assert(cg_ret == kCGErrorSuccess);

    return true;
}

static void p1_display_video_source_frame(P1VideoSource *_source)
{
    P1DisplayVideoSource *source = (P1DisplayVideoSource *)_source;
    IOSurfaceRef frame;

    pthread_mutex_lock(&source->frame_lock);
    frame = source->frame;
    if (frame) {
        CFRetain(frame);
        IOSurfaceIncrementUseCount(frame);
    }
    pthread_mutex_unlock(&source->frame_lock);

    if (!frame)
        return;

    p1_video_frame_iosurface(_source, frame);

    IOSurfaceDecrementUseCount(frame);
    CFRelease(frame);
}

static void p1_display_video_source_stop(P1VideoSource *_source)
{
    P1DisplayVideoSource *source = (P1DisplayVideoSource *)_source;

    CGError cg_ret = CGDisplayStreamStop(source->display_stream);
    assert(cg_ret == kCGErrorSuccess);
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
