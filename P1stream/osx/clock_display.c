// Video clock based on display refresh rate, using CVDisplayLink.

#include <stdio.h>
#include <CoreVideo/CoreVideo.h>

#include "p1stream.h"


typedef struct _P1DisplayVideoClock P1DisplayVideoClock;

struct _P1DisplayVideoClock {
    P1VideoClock super;

    CVDisplayLinkRef display_link;
};

static void p1_display_video_clock_free(P1VideoClock *_clock);
static bool p1_display_video_clock_start(P1VideoClock *_clock);
static void p1_display_video_clock_stop(P1VideoClock *_clock);
static CVReturn p1_display_video_clock_callback(
    CVDisplayLinkRef displayLink,
    const CVTimeStamp *inNow,
    const CVTimeStamp *inOutputTime,
    CVOptionFlags flagsIn,
    CVOptionFlags *flagsOut,
    void *displayLinkContext);


P1VideoClock *p1_display_video_clock_create()
{
    CVReturn ret;

    P1DisplayVideoClock *clock = calloc(1, sizeof(P1DisplayVideoClock));
    assert(clock != NULL);

    P1VideoClock *_clock = (P1VideoClock *) clock;
    _clock->free = p1_display_video_clock_free;
    _clock->start = p1_display_video_clock_start;
    _clock->stop = p1_display_video_clock_stop;

    const CGDirectDisplayID display_id = kCGDirectMainDisplay;

    ret = CVDisplayLinkCreateWithCGDisplay(display_id, &clock->display_link);
    assert(ret == kCVReturnSuccess);
    ret = CVDisplayLinkSetOutputCallback(clock->display_link, p1_display_video_clock_callback, clock);
    assert(ret == kCVReturnSuccess);

    return _clock;
}

static void p1_display_video_clock_free(P1VideoClock *_clock)
{
    P1DisplayVideoClock *clock = (P1DisplayVideoClock *) _clock;

    CFRelease(clock->display_link);
}

static bool p1_display_video_clock_start(P1VideoClock *_clock)
{
    P1DisplayVideoClock *clock = (P1DisplayVideoClock *) _clock;

    CVReturn cv_ret = CVDisplayLinkStart(clock->display_link);
    assert(cv_ret == kCVReturnSuccess);

    return true;
}

static void p1_display_video_clock_stop(P1VideoClock *_clock)
{
    P1DisplayVideoClock *clock = (P1DisplayVideoClock *) _clock;

    CVReturn cv_ret = CVDisplayLinkStop(clock->display_link);
    assert(cv_ret == kCVReturnSuccess);
}

static CVReturn p1_display_video_clock_callback(
    CVDisplayLinkRef displayLink,
    const CVTimeStamp *inNow,
    const CVTimeStamp *inOutputTime,
    CVOptionFlags flagsIn,
    CVOptionFlags *flagsOut,
    void *displayLinkContext)
{
    P1VideoClock *_clock = (P1VideoClock *) displayLinkContext;

    p1_video_clock_tick(_clock, inNow->hostTime);

    return kCVReturnSuccess;
}
