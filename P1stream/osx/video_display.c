#include "p1stream.h"

#include <pthread.h>
#include <dispatch/dispatch.h>
#include <CoreGraphics/CoreGraphics.h>

typedef struct _P1DisplayVideoSource P1DisplayVideoSource;

struct _P1DisplayVideoSource {
    P1VideoSource super;

    CGDirectDisplayID display_id;

    dispatch_queue_t dispatch;
    CGDisplayStreamRef display_stream;

    IOSurfaceRef frame;
};

static bool p1_display_video_source_start(P1PluginElement *pel);
static void p1_display_video_source_stop(P1PluginElement *pel);
static void p1_display_video_source_frame(P1VideoSource *vsrc);
static void p1_display_video_source_callback(
    P1DisplayVideoSource *dvsrc,
    CGDisplayStreamFrameStatus status,
    IOSurfaceRef frame);


P1VideoSource *p1_display_video_source_create(P1Config *cfg, P1ConfigSection *sect)
{
    P1DisplayVideoSource *dvsrc = calloc(1, sizeof(P1DisplayVideoSource));
    P1VideoSource *vsrc = (P1VideoSource *) dvsrc;
    P1PluginElement *pel = (P1PluginElement *) dvsrc;
    assert(dvsrc != NULL);

    p1_video_source_init(vsrc, cfg, sect);

    pel->start = p1_display_video_source_start;
    pel->stop = p1_display_video_source_stop;
    vsrc->frame = p1_display_video_source_frame;

    // FIXME: configurable
    dvsrc->display_id = kCGDirectMainDisplay;

    return vsrc;
}

static bool p1_display_video_source_start(P1PluginElement *pel)
{
    P1Element *el = (P1Element *) pel;
    P1DisplayVideoSource *dvsrc = (P1DisplayVideoSource *) pel;

    p1_set_state(el, P1_OTYPE_VIDEO_SOURCE, P1_STATE_STARTING);

    size_t width  = CGDisplayPixelsWide(dvsrc->display_id);
    size_t height = CGDisplayPixelsHigh(dvsrc->display_id);

    dvsrc->dispatch = dispatch_queue_create("video_desktop", DISPATCH_QUEUE_SERIAL);
    assert(dvsrc->dispatch != NULL);

    dvsrc->display_stream = CGDisplayStreamCreateWithDispatchQueue(
        dvsrc->display_id, width, height, 'BGRA', NULL, dvsrc->dispatch, ^(
            CGDisplayStreamFrameStatus status,
            uint64_t displayTime,
            IOSurfaceRef frameSurface,
            CGDisplayStreamUpdateRef updateRef)
        {
            p1_display_video_source_callback(dvsrc, status, frameSurface);
        });
    assert(dvsrc->display_stream != NULL);

    CGError cg_ret = CGDisplayStreamStart(dvsrc->display_stream);
    assert(cg_ret == kCGErrorSuccess);

    return true;
}

static void p1_display_video_source_stop(P1PluginElement *pel)
{
    P1Element *el = (P1Element *) pel;
    P1DisplayVideoSource *dvsrc = (P1DisplayVideoSource *) pel;

    p1_set_state(el, P1_OTYPE_VIDEO_SOURCE, P1_STATE_STOPPING);

    CGError cg_ret = CGDisplayStreamStop(dvsrc->display_stream);
    assert(cg_ret == kCGErrorSuccess);
}

static void p1_display_video_source_frame(P1VideoSource *vsrc)
{
    P1DisplayVideoSource *dvsrc = (P1DisplayVideoSource *) vsrc;

    if (dvsrc->frame)
        p1_video_source_frame_iosurface(vsrc, dvsrc->frame);
}

static void p1_display_video_source_callback(
    P1DisplayVideoSource *dvsrc,
    CGDisplayStreamFrameStatus status,
    IOSurfaceRef frame)
{
    P1Element *el = (P1Element *) dvsrc;

    p1_element_lock(el);

    // Ditch any previous frame, unless it's the same.
    // This also doubles as cleanup when stopping.
    if (dvsrc->frame && status != kCGDisplayStreamFrameStatusFrameIdle) {
        IOSurfaceDecrementUseCount(dvsrc->frame);
        CFRelease(dvsrc->frame);
        dvsrc->frame = NULL;
    }

    // A new frame arrived, retain it.
    if (status == kCGDisplayStreamFrameStatusFrameComplete) {
        dvsrc->frame = frame;
        CFRetain(frame);
        IOSurfaceIncrementUseCount(frame);
    }

    // State handling.
    if (status == kCGDisplayStreamFrameStatusStopped) {
        if (el->state == P1_STATE_STOPPING) {
            CFRelease(dvsrc->display_stream);

            dispatch_release(dvsrc->dispatch);

            p1_set_state(el, P1_OTYPE_VIDEO_SOURCE, P1_STATE_IDLE);
        }
        else {
            p1_log(el->ctx, P1_LOG_ERROR, "Display stream stopped.");
            abort();
        }
    }
    else {
        if (el->state == P1_STATE_STARTING)
            p1_set_state(el, P1_OTYPE_VIDEO_SOURCE, P1_STATE_RUNNING);
    }

    p1_element_unlock(el);
}
