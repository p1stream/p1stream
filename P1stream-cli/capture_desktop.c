#include <stdio.h>
#include <mach/mach_time.h>
#include <dispatch/dispatch.h>
#include <CoreGraphics/CoreGraphics.h>

#include "output.h"

static void p1_capture_desktop_frame(
    CGDisplayStreamFrameStatus status, uint64_t displayTime,
    IOSurfaceRef frameSurface, CGDisplayStreamUpdateRef updateRef);

static struct {
    CGDisplayStreamRef display_stream;

    uint64_t last_time;
    uint64_t frame_period;
} state;

static const size_t fps = 60;


int p1_capture_desktop_start()
{
    int res = 0;

    dispatch_queue_t queue = dispatch_get_main_queue();
    const CGDirectDisplayID display_id = kCGDirectMainDisplay;
    size_t width  = CGDisplayPixelsWide(display_id);
    size_t height = CGDisplayPixelsHigh(display_id);

    state.display_stream = CGDisplayStreamCreateWithDispatchQueue(
        display_id, width, height, 'BGRA', NULL, queue,
        ^(CGDisplayStreamFrameStatus status, uint64_t displayTime, IOSurfaceRef frameSurface, CGDisplayStreamUpdateRef updateRef) {
            p1_capture_desktop_frame(status, displayTime, frameSurface, updateRef);
        });
    if (state.display_stream) {
        res = CGDisplayStreamStart(state.display_stream) == kCGErrorSuccess;
        if (!res) {
            CFRelease(state.display_stream);
            state.display_stream = NULL;
        }
    }

    mach_timebase_info_data_t base;
    mach_timebase_info(&base);
    state.frame_period = 1000000000 / fps * base.denom / base.numer;

    return res;
}

static void p1_capture_desktop_frame(
    CGDisplayStreamFrameStatus status, uint64_t displayTime,
    IOSurfaceRef frameSurface, CGDisplayStreamUpdateRef updateRef)
{
    switch (status) {
        case kCGDisplayStreamFrameStatusFrameComplete:
            p1_output_video_iosurface(frameSurface);
            break;
        case kCGDisplayStreamFrameStatusFrameIdle:
            p1_output_video_idle();
            break;
        case kCGDisplayStreamFrameStatusFrameBlank:
            p1_output_video_blank();
            break;
        case kCGDisplayStreamFrameStatusStopped:
            printf("Display stream stopped.");
            abort();
    }

    // When idle, we get frames at a lower rate, apparently 15hz.
    // Pad with idle frames.
    if (state.last_time == 0) {
        state.last_time = displayTime;
    }
    else {
        state.last_time += state.frame_period;
        while (state.last_time < displayTime) {
            p1_output_video_idle();
            state.last_time += state.frame_period;
        }
    }
}
