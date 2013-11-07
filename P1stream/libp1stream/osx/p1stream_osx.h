#ifndef p1stream_osx_h
#define p1stream_osx_h

#ifdef __OBJC__
#   include <Foundation/Foundation.h>
#endif

#include <CoreFoundation/CoreFoundation.h>
#include <IOSurface/IOSurface.h>


typedef struct _P1PreviewRawData P1PreviewRawData;


// Convenience logging methods that build on p1_log.
#ifdef __OBJC__
void p1_log_ns_string(P1Object *obj, P1LogLevel level, NSString *str);
void p1_log_ns_error(P1Object *obj, P1LogLevel level, NSError *err);
#endif
void p1_log_cf_error(P1Object *obj, P1LogLevel level, CFErrorRef err);
void p1_log_os_status(P1Object *obj, P1LogLevel level, OSStatus status);


// Fast path for OS X video sources that can provide an IOSurface.
bool p1_video_source_frame_iosurface(P1VideoSource *vsrc, IOSurfaceRef buffer);

// Preview callback type where data is the below struct, allowing access to
// raw pixel data. Pixel data is in little-endian BGRA format. Data is not
// should not be accessed after the callback returns.
#define P1_PREVIEW_RAW_DATA 0
struct _P1PreviewRawData {
    size_t width;
    size_t height;
    const uint8_t *data;
};

// Preview callback type where data is an IOSurfaceRef. An additional callback
// is made before the IOSurface is released, with data set to NULL.
#define P1_PREVIEW_IOSURFACE 1


// Configuration backed by a property list. Uses Core Foundation.
#ifdef __OBJC__
P1Config *p1_plist_config_create(NSDictionary *dict);
#endif


// Audio source using a system audio input. Uses Audio Toolbox.
P1AudioSource *p1_input_audio_source_create(P1Context *ctx);


// Video clock timed to the refresh rate of a display. Uses Core Video.
P1VideoClock *p1_display_video_clock_create(P1Context *ctx);


// Video source that captures from a display. Uses Core Graphics.
P1VideoSource *p1_display_video_source_create(P1Context *ctx);

// Video source that opens a system capture device. Uses AV Foundation.
P1VideoSource *p1_capture_video_source_create(P1Context *ctx);

#endif
