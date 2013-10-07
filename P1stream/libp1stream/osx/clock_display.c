#include "p1stream.h"

#include <CoreVideo/CoreVideo.h>


typedef struct _P1DisplayVideoClock P1DisplayVideoClock;

struct _P1DisplayVideoClock {
    P1VideoClock super;

    CGDirectDisplayID display_id;
    uint8_t divisor;

    CVDisplayLinkRef display_link;
    uint8_t skip_counter;
};

static bool p1_display_video_clock_init(P1DisplayVideoClock *dvclock);
static void p1_display_video_clock_start(P1Plugin *pel);
static void p1_display_video_clock_stop(P1Plugin *pel);
static void p1_display_video_clock_kill_session(P1DisplayVideoClock *dvclock);
static CVReturn p1_display_video_clock_callback(
    CVDisplayLinkRef displayLink,
    const CVTimeStamp *inNow,
    const CVTimeStamp *inOutputTime,
    CVOptionFlags flagsIn,
    CVOptionFlags *flagsOut,
    void *displayLinkContext);


P1VideoClock *p1_display_video_clock_create()
{
    P1DisplayVideoClock *dvclock = calloc(1, sizeof(P1DisplayVideoClock));

    if (dvclock) {
        if (!p1_display_video_clock_init(dvclock)) {
            free(dvclock);
            dvclock = NULL;
        }
    }

    return (P1VideoClock *) dvclock;
}

static bool p1_display_video_clock_init(P1DisplayVideoClock *dvclock)
{
    P1VideoClock *vclock = (P1VideoClock *) dvclock;
    P1Plugin *pel = (P1Plugin *) dvclock;

    if (!p1_video_clock_init(vclock))
        return false;

    pel->start = p1_display_video_clock_start;
    pel->stop = p1_display_video_clock_stop;

    // FIXME: configurable
    dvclock->display_id = kCGDirectMainDisplay;
    dvclock->divisor = 2;

    return true;
}

static void p1_display_video_clock_start(P1Plugin *pel)
{
    P1Object *obj = (P1Object *) pel;
    P1DisplayVideoClock *dvclock = (P1DisplayVideoClock *) pel;
    CVReturn ret;

    p1_object_set_state(obj, P1_STATE_STARTING);

    dvclock->skip_counter = 0;

    ret = CVDisplayLinkCreateWithCGDisplay(dvclock->display_id, &dvclock->display_link);
    if (ret != kCVReturnSuccess)
        goto halt;

    ret = CVDisplayLinkSetOutputCallback(dvclock->display_link, p1_display_video_clock_callback, dvclock);
    if (ret != kCVReturnSuccess)
        goto halt;

    // Async, final state change happens in the callback.
    ret = CVDisplayLinkStart(dvclock->display_link);
    if (ret != kCVReturnSuccess)
        goto halt;

    return;

halt:
    p1_log(obj, P1_LOG_ERROR, "Failed to start display link: Core Video error %d", ret);

    obj->flags |= P1_FLAG_ERROR;
    p1_object_set_state(obj, P1_STATE_STOPPING);

    p1_display_video_clock_kill_session(dvclock);

    p1_object_set_state(obj, P1_STATE_IDLE);
}

static void p1_display_video_clock_stop(P1Plugin *pel)
{
    P1Object *obj = (P1Object *) pel;
    P1DisplayVideoClock *dvclock = (P1DisplayVideoClock *) pel;
    CVReturn ret;

    p1_object_set_state(obj, P1_STATE_STOPPING);

    // Stop the display link. This apparently blocks.
    p1_object_unlock(obj);
    ret = CVDisplayLinkStop(dvclock->display_link);
    p1_object_lock(obj);

    if (ret != kCVReturnSuccess) {
        p1_log(obj, P1_LOG_ERROR, "Failed to stop display link: Core Video error %d", ret);
        obj->flags |= P1_FLAG_ERROR;
        p1_object_set_state(obj, P1_STATE_STOPPING);
    }

    p1_display_video_clock_kill_session(dvclock);

    p1_object_set_state(obj, P1_STATE_IDLE);
}

static void p1_display_video_clock_kill_session(P1DisplayVideoClock *dvclock)
{
    if (dvclock->display_link) {
        CFRelease(dvclock->display_link);
        dvclock->display_link = NULL;
    }
}

static CVReturn p1_display_video_clock_callback(
    CVDisplayLinkRef displayLink,
    const CVTimeStamp *inNow,
    const CVTimeStamp *inOutputTime,
    CVOptionFlags flagsIn,
    CVOptionFlags *flagsOut,
    void *displayLinkContext)
{
    P1DisplayVideoClock *dvclock = (P1DisplayVideoClock *) displayLinkContext;
    P1VideoClock *vclock = (P1VideoClock *) displayLinkContext;
    P1Object *obj = (P1Object *) displayLinkContext;

    p1_object_lock(obj);

    if (obj->state == P1_STATE_STARTING) {
        // Get the display refresh period.
        double period = CVDisplayLinkGetActualOutputVideoRefreshPeriod(dvclock->display_link);
        if (period == 0.0)
            goto end;

        // Set the frame rate based on this and the divisor.
        vclock->fps_num = (uint32_t) round(1.0 / period);
        vclock->fps_den = dvclock->divisor;

        // Report running.
        p1_object_set_state(obj, P1_STATE_RUNNING);
    }

    if (obj->state == P1_STATE_RUNNING) {
        // Skip tick based on divisor.
        if (dvclock->skip_counter >= dvclock->divisor)
            dvclock->skip_counter = 0;
        if (dvclock->skip_counter++ != 0)
            goto end;

        p1_object_unlock(obj);
        p1_video_clock_tick(vclock, inNow->hostTime);
        return kCVReturnSuccess;
    }

end:
    p1_object_unlock(obj);

    return kCVReturnSuccess;
}
