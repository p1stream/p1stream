// Video clock based on display refresh rate, using CVDisplayLink.

#include <stdio.h>
#include <CoreVideo/CoreVideo.h>

#include "p1stream.h"


typedef struct _P1DisplayVideoClock P1DisplayVideoClock;

struct _P1DisplayVideoClock {
    P1VideoClock super;

    CVDisplayLinkRef display_link;
};

static void p1_display_video_clock_free(P1VideoClock *vclock);
static bool p1_display_video_clock_start(P1VideoClock *vclock);
static void p1_display_video_clock_stop(P1VideoClock *vclock);
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
    P1VideoClock *vclock = (P1VideoClock *) dvclock;
    assert(vclock != NULL);

    vclock->free = p1_display_video_clock_free;
    vclock->start = p1_display_video_clock_start;
    vclock->stop = p1_display_video_clock_stop;

    CVReturn ret;
    const CGDirectDisplayID display_id = kCGDirectMainDisplay;

    ret = CVDisplayLinkCreateWithCGDisplay(display_id, &dvclock->display_link);
    assert(ret == kCVReturnSuccess);
    ret = CVDisplayLinkSetOutputCallback(dvclock->display_link, p1_display_video_clock_callback, vclock);
    assert(ret == kCVReturnSuccess);

    return vclock;
}

static void p1_display_video_clock_free(P1VideoClock *vclock)
{
    P1DisplayVideoClock *dvclock = (P1DisplayVideoClock *) vclock;

    CFRelease(dvclock->display_link);
}

static bool p1_display_video_clock_start(P1VideoClock *vclock)
{
    P1DisplayVideoClock *dvclock = (P1DisplayVideoClock *) vclock;

    CVReturn cv_ret = CVDisplayLinkStart(dvclock->display_link);
    assert(cv_ret == kCVReturnSuccess);

    // FIXME: Should we wait for anything?
    vclock->state = P1StateRunning;

    return true;
}

static void p1_display_video_clock_stop(P1VideoClock *vclock)
{
    P1DisplayVideoClock *dvclock = (P1DisplayVideoClock *) vclock;

    CVReturn cv_ret = CVDisplayLinkStop(dvclock->display_link);
    assert(cv_ret == kCVReturnSuccess);

    // FIXME: Should we wait for anything?
    vclock->state = P1StateIdle;
}

static CVReturn p1_display_video_clock_callback(
    CVDisplayLinkRef displayLink,
    const CVTimeStamp *inNow,
    const CVTimeStamp *inOutputTime,
    CVOptionFlags flagsIn,
    CVOptionFlags *flagsOut,
    void *displayLinkContext)
{
    P1VideoClock *vclock = (P1VideoClock *) displayLinkContext;

    p1_clock_tick(vclock, inNow->hostTime);

    return kCVReturnSuccess;
}
