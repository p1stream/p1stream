#include "p1stream.h"

#define GL_DO_NOT_WARN_IF_MULTI_GL_VERSION_HEADERS_INCLUDED

#include <CoreVideo/CoreVideo.h>


typedef struct _P1DisplayVideoClock P1DisplayVideoClock;

struct _P1DisplayVideoClock {
    P1VideoClock super;

    CGDirectDisplayID display_id;
    uint8_t divisor;

    CVDisplayLinkRef display_link;
    uint8_t skip_counter;
};

static bool p1_display_video_clock_start(P1PluginElement *pel);
static void p1_display_video_clock_stop(P1PluginElement *pel);
static CVReturn p1_display_video_clock_callback(
    CVDisplayLinkRef displayLink,
    const CVTimeStamp *inNow,
    const CVTimeStamp *inOutputTime,
    CVOptionFlags flagsIn,
    CVOptionFlags *flagsOut,
    void *displayLinkContext);


P1VideoClock *p1_display_video_clock_create(P1Config *cfg, P1ConfigSection *sect)
{
    P1DisplayVideoClock *dvclock = calloc(1, sizeof(P1DisplayVideoClock));
    P1VideoClock *vclock = (P1VideoClock *) dvclock;
    P1PluginElement *pel = (P1PluginElement *) dvclock;
    assert(vclock != NULL);

    p1_video_clock_init(vclock, cfg, sect);

    pel->start = p1_display_video_clock_start;
    pel->stop = p1_display_video_clock_stop;

    // FIXME: configurable
    dvclock->display_id = kCGDirectMainDisplay;
    dvclock->divisor = 2;

    return vclock;
}

static bool p1_display_video_clock_start(P1PluginElement *pel)
{
    P1Element *el = (P1Element *) pel;
    P1DisplayVideoClock *dvclock = (P1DisplayVideoClock *) pel;
    CVReturn ret;

    p1_element_set_state(el, P1_OTYPE_VIDEO_CLOCK, P1_STATE_STARTING);

    dvclock->skip_counter = 0;

    ret = CVDisplayLinkCreateWithCGDisplay(dvclock->display_id, &dvclock->display_link);
    assert(ret == kCVReturnSuccess);

    ret = CVDisplayLinkSetOutputCallback(dvclock->display_link, p1_display_video_clock_callback, dvclock);
    assert(ret == kCVReturnSuccess);

    CVReturn cv_ret = CVDisplayLinkStart(dvclock->display_link);
    assert(cv_ret == kCVReturnSuccess);

    return true;
}

static void p1_display_video_clock_stop(P1PluginElement *pel)
{
    P1Element *el = (P1Element *) pel;
    P1DisplayVideoClock *dvclock = (P1DisplayVideoClock *) pel;

    p1_element_set_state(el, P1_OTYPE_VIDEO_CLOCK, P1_STATE_STOPPING);

    CVReturn cv_ret = CVDisplayLinkStop(dvclock->display_link);
    assert(cv_ret == kCVReturnSuccess);

    CFRelease(dvclock->display_link);

    // FIXME: Should we wait for anything?
    p1_element_set_state(el, P1_OTYPE_VIDEO_CLOCK, P1_STATE_IDLE);
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
    P1Element *el = (P1Element *) displayLinkContext;

    p1_element_lock(el);

    if (el->state == P1_STATE_STARTING) {
        // Get the display refresh period.
        double period = CVDisplayLinkGetActualOutputVideoRefreshPeriod(dvclock->display_link);
        if (period == 0.0)
            goto end;

        // Set the frame rate based on this and the divisor.
        vclock->fps_num = (uint32_t) round(1.0 / period);
        vclock->fps_den = dvclock->divisor;

        // Report running.
        p1_element_set_state(el, P1_OTYPE_VIDEO_CLOCK, P1_STATE_RUNNING);
    }

    if (el->state == P1_STATE_RUNNING) {
        // Skip tick based on divisor.
        if (dvclock->skip_counter >= dvclock->divisor)
            dvclock->skip_counter = 0;
        if (dvclock->skip_counter++ != 0)
            goto end;

        p1_video_clock_tick(vclock, inNow->hostTime);
    }

end:
    p1_element_unlock(el);

    return kCVReturnSuccess;
}
