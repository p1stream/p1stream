#include "p1stream.h"

#include <pthread.h>
#include <dispatch/dispatch.h>
#include <CoreGraphics/CoreGraphics.h>

typedef struct _P1DisplayVideoSource P1DisplayVideoSource;

struct _P1DisplayVideoSource {
    P1VideoSource super;

    CGDirectDisplayID cfg_display_id;

    CGDirectDisplayID display_id;

    dispatch_queue_t dispatch;
    CGDisplayStreamRef display_stream;

    IOSurfaceRef frame;
};

static bool p1_display_video_source_init(P1DisplayVideoSource *dvsrc, P1Context *ctx);
static void p1_display_video_source_config(P1Plugin *pel, P1Config *cfg);
static void p1_display_video_source_start(P1Plugin *pel);
static void p1_display_video_source_stop(P1Plugin *pel);
static void p1_display_video_source_kill_session(P1DisplayVideoSource *dvsrc);
static void p1_display_video_source_halt(P1DisplayVideoSource *dvsrc);
static bool p1_display_video_source_frame(P1VideoSource *vsrc);
static void p1_display_video_source_callback(
    P1DisplayVideoSource *dvsrc,
    CGDisplayStreamFrameStatus status,
    IOSurfaceRef frame);


P1VideoSource *p1_display_video_source_create(P1Context *ctx)
{
    P1DisplayVideoSource *dvsrc = calloc(1, sizeof(P1DisplayVideoSource));

    if (dvsrc != NULL) {
        if (!p1_display_video_source_init(dvsrc, ctx)) {
            free(dvsrc);
            dvsrc = NULL;
        }
    }

    return (P1VideoSource *) dvsrc;
}

static bool p1_display_video_source_init(P1DisplayVideoSource *dvsrc, P1Context *ctx)
{
    P1VideoSource *vsrc = (P1VideoSource *) dvsrc;
    P1Plugin *pel = (P1Plugin *) dvsrc;

    if (!p1_video_source_init(vsrc, ctx))
        return false;

    pel->config = p1_display_video_source_config;
    pel->start = p1_display_video_source_start;
    pel->stop = p1_display_video_source_stop;
    vsrc->frame = p1_display_video_source_frame;

    return true;
}

static void p1_display_video_source_config(P1Plugin *pel, P1Config *cfg)
{
    P1DisplayVideoSource *dvsrc = (P1DisplayVideoSource *) pel;
    P1Object *obj = (P1Object *) pel;

    if (cfg->get_uint32(cfg, "display", &dvsrc->cfg_display_id))
        dvsrc->display_id = kCGDirectMainDisplay;

    if (dvsrc->cfg_display_id != dvsrc->display_id)
        p1_object_set_flag(obj, P1_FLAG_NEEDS_RESTART);
}

static void p1_display_video_source_start(P1Plugin *pel)
{
    P1Object *obj = (P1Object *) pel;
    P1DisplayVideoSource *dvsrc = (P1DisplayVideoSource *) pel;
    CGError ret = kCGErrorSuccess;
    size_t width  = CGDisplayPixelsWide(dvsrc->display_id);
    size_t height = CGDisplayPixelsHigh(dvsrc->display_id);

    dvsrc->dispatch = dispatch_queue_create("video_desktop", DISPATCH_QUEUE_SERIAL);
    if (dvsrc->dispatch == NULL) {
        p1_log(obj, P1_LOG_ERROR, "Failed to create dispatch queue");
        p1_display_video_source_halt(dvsrc);
        return;
    }

    dvsrc->display_id = dvsrc->cfg_display_id;
    dvsrc->display_stream = CGDisplayStreamCreateWithDispatchQueue(
        dvsrc->display_id, width, height, 'BGRA', NULL, dvsrc->dispatch, ^(
            CGDisplayStreamFrameStatus status,
            uint64_t displayTime,
            IOSurfaceRef frameSurface,
            CGDisplayStreamUpdateRef updateRef)
        {
            p1_display_video_source_callback(dvsrc, status, frameSurface);
        });
    if (dvsrc->display_stream == NULL) {
        p1_log(obj, P1_LOG_ERROR, "Failed to create display stream");
        p1_display_video_source_halt(dvsrc);
        return;
    }

    ret = CGDisplayStreamStart(dvsrc->display_stream);
    if (ret != kCGErrorSuccess) {
        p1_log(obj, P1_LOG_ERROR, "Failed to start display stream: Core Graphics error %d", ret);
        p1_display_video_source_halt(dvsrc);
        return;
    }

    obj->state.current = P1_STATE_STARTING;
    p1_object_notify(obj);

    return;
}

static void p1_display_video_source_stop(P1Plugin *pel)
{
    P1Object *obj = (P1Object *) pel;
    P1DisplayVideoSource *dvsrc = (P1DisplayVideoSource *) pel;

    obj->state.current = P1_STATE_STOPPING;
    p1_object_notify(obj);

    CGError ret = CGDisplayStreamStop(dvsrc->display_stream);
    if (ret != kCGErrorSuccess) {
        p1_log(obj, P1_LOG_ERROR, "Failed to stop display stream: Core Graphics error %d", ret);
        p1_display_video_source_halt(dvsrc);
    }
}

static void p1_display_video_source_kill_session(P1DisplayVideoSource *dvsrc)
{
    CFRelease(dvsrc->display_stream);
    dvsrc->display_stream = NULL;

    dispatch_release(dvsrc->dispatch);
    dvsrc->dispatch = NULL;
}

static void p1_display_video_source_halt(P1DisplayVideoSource *dvsrc)
{
    P1Object *obj = (P1Object *) dvsrc;

    p1_display_video_source_kill_session(dvsrc);

    obj->state.current = P1_STATE_IDLE;
    obj->state.flags |= P1_FLAG_ERROR;
    p1_object_notify(obj);
}

static bool p1_display_video_source_frame(P1VideoSource *vsrc)
{
    P1DisplayVideoSource *dvsrc = (P1DisplayVideoSource *) vsrc;

    if (dvsrc->frame)
        return p1_video_source_frame_iosurface(vsrc, dvsrc->frame);
    else
        return true;
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

        if (obj->state.current == P1_STATE_STOPPING) {
            obj->state.current = P1_STATE_IDLE;
            p1_object_notify(obj);
        }
        else {
            p1_log(obj, P1_LOG_ERROR, "Display stream stopped itself");
            p1_display_video_source_halt(dvsrc);
        }
    }
    else {
        if (obj->state.current == P1_STATE_STARTING) {
            obj->state.current = P1_STATE_RUNNING;
            p1_object_notify(obj);
        }
    }

    p1_object_unlock(obj);
}
