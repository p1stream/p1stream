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

static bool p1_display_video_source_init(P1DisplayVideoSource *dvsrc, P1Config *cfg, P1ConfigSection *sect);
static void p1_display_video_source_start(P1Plugin *pel);
static void p1_display_video_source_stop(P1Plugin *pel);
static void p1_display_video_source_kill_session(P1DisplayVideoSource *dvsrc);
static void p1_display_video_source_frame(P1VideoSource *vsrc);
static void p1_display_video_source_callback(
    P1DisplayVideoSource *dvsrc,
    CGDisplayStreamFrameStatus status,
    IOSurfaceRef frame);


P1VideoSource *p1_display_video_source_create(P1Config *cfg, P1ConfigSection *sect)
{
    P1DisplayVideoSource *dvsrc = calloc(1, sizeof(P1DisplayVideoSource));

    if (dvsrc != NULL) {
        if (!p1_display_video_source_init(dvsrc, cfg, sect)) {
            free(dvsrc);
            dvsrc = NULL;
        }
    }

    return (P1VideoSource *) dvsrc;
}

static bool p1_display_video_source_init(P1DisplayVideoSource *dvsrc, P1Config *cfg, P1ConfigSection *sect)
{
    P1VideoSource *vsrc = (P1VideoSource *) dvsrc;
    P1Plugin *pel = (P1Plugin *) dvsrc;

    if (!p1_video_source_init(vsrc, cfg, sect))
        return false;

    pel->start = p1_display_video_source_start;
    pel->stop = p1_display_video_source_stop;
    vsrc->frame = p1_display_video_source_frame;

    // FIXME: configurable
    dvsrc->display_id = kCGDirectMainDisplay;

    return true;
}

static void p1_display_video_source_start(P1Plugin *pel)
{
    P1Object *obj = (P1Object *) pel;
    P1DisplayVideoSource *dvsrc = (P1DisplayVideoSource *) pel;
    CGError ret = kCGErrorSuccess;
    size_t width  = CGDisplayPixelsWide(dvsrc->display_id);
    size_t height = CGDisplayPixelsHigh(dvsrc->display_id);

    p1_object_set_state(obj, P1_STATE_STARTING);

    dvsrc->dispatch = dispatch_queue_create("video_desktop", DISPATCH_QUEUE_SERIAL);
    if (dvsrc->dispatch == NULL)
        goto halt;

    dvsrc->display_stream = CGDisplayStreamCreateWithDispatchQueue(
        dvsrc->display_id, width, height, 'BGRA', NULL, dvsrc->dispatch, ^(
            CGDisplayStreamFrameStatus status,
            uint64_t displayTime,
            IOSurfaceRef frameSurface,
            CGDisplayStreamUpdateRef updateRef)
        {
            p1_display_video_source_callback(dvsrc, status, frameSurface);
        });
    if (dvsrc->display_stream == NULL)
        goto halt;

    ret = CGDisplayStreamStart(dvsrc->display_stream);
    if (ret != kCGErrorSuccess)
        goto halt;

    return;

halt:
    p1_log(obj, P1_LOG_ERROR, "Failed to setup display stream\n");
    // FIXME: log error
    p1_display_video_source_kill_session(dvsrc);
    p1_object_set_state(obj, P1_STATE_HALTED);
}

static void p1_display_video_source_stop(P1Plugin *pel)
{
    P1Object *obj = (P1Object *) pel;
    P1DisplayVideoSource *dvsrc = (P1DisplayVideoSource *) pel;

    p1_object_set_state(obj, P1_STATE_STOPPING);

    CGError ret = CGDisplayStreamStop(dvsrc->display_stream);
    if (ret != kCGErrorSuccess) {
        p1_log(obj, P1_LOG_ERROR, "Failed to stop display stream\n");
        // FIXME: log error
        p1_display_video_source_kill_session(dvsrc);
        p1_object_set_state(obj, P1_STATE_HALTED);
    }
}

static void p1_display_video_source_kill_session(P1DisplayVideoSource *dvsrc)
{
    CFRelease(dvsrc->display_stream);
    dvsrc->display_stream = NULL;

    dispatch_release(dvsrc->dispatch);
    dvsrc->dispatch = NULL;
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
    P1Object *obj = (P1Object *) dvsrc;

    p1_object_lock(obj);

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
        p1_display_video_source_kill_session(dvsrc);

        if (obj->state == P1_STATE_STOPPING) {
            p1_object_set_state(obj, P1_STATE_IDLE);
        }
        else {
            p1_log(obj, P1_LOG_ERROR, "Display stream stopped itself\n");
            p1_object_set_state(obj, P1_STATE_HALTED);
        }
    }
    else {
        if (obj->state == P1_STATE_STARTING)
            p1_object_set_state(obj, P1_STATE_RUNNING);
    }

    p1_object_unlock(obj);
}
