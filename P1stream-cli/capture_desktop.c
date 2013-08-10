#include <stdio.h>
#include <dispatch/dispatch.h>
#include <CoreGraphics/CoreGraphics.h>

#include "output.h"

static void p1_capture_desktop_frame(
    CGDisplayStreamFrameStatus status, uint64_t displayTime,
    IOSurfaceRef frameSurface, CGDisplayStreamUpdateRef updateRef);


static struct {
    CGDisplayStreamRef display_stream;
} state;

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
    
    return res;
}

static void p1_capture_desktop_frame(
    CGDisplayStreamFrameStatus status, uint64_t displayTime,
    IOSurfaceRef frameSurface, CGDisplayStreamUpdateRef updateRef)
{
    if (status == kCGDisplayStreamFrameStatusFrameComplete) {
        p1_output_video_iosurface(frameSurface);
    }
    else if (status == kCGDisplayStreamFrameStatusStopped) {
        printf("Display stream stopped.");
        abort();
    }
}
