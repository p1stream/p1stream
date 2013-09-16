#ifndef p1stream_osx_h
#define p1stream_osx_h

#ifdef __OBJC__
#   include <Foundation/Foundation.h>
#endif

#include <CoreFoundation/CoreFoundation.h>
#include <IOSurface/IOSurface.h>


// Convenience logging methods that build on p1_log.
#ifdef __OBJC__
void p1_log_ns_string(P1Object *obj, P1LogLevel level, NSString *str);
void p1_log_ns_error(P1Object *obj, P1LogLevel level, NSError *err);
#endif
void p1_log_cf_error(P1Object *obj, P1LogLevel level, CFErrorRef err);
void p1_log_os_status(P1Object *obj, P1LogLevel level, OSStatus status);


// Fast path for OS X video sources that can provide an IOSurface.
bool p1_video_source_frame_iosurface(P1VideoSource *vsrc, IOSurfaceRef buffer);


// Configuration backed by a property list. Uses Core Foundation.
P1Config *p1_plist_config_create(CFDictionaryRef root);
P1Config *p1_plist_config_create_from_file(const char *file);


// Audio source using a system audio input. Uses Audio Toolbox.
P1AudioSource *p1_input_audio_source_create(P1Config *cfg, P1ConfigSection *sect);


// Video clock timed to the refresh rate of a display. Uses Core Video.
P1VideoClock *p1_display_video_clock_create(P1Config *cfg, P1ConfigSection *sect);


// Video source that captures from a display. Uses Core Graphics.
P1VideoSource *p1_display_video_source_create(P1Config *cfg, P1ConfigSection *sect);

// Video source that opens a system capture device. Uses AV Foundation.
P1VideoSource *p1_capture_video_source_create(P1Config *cfg, P1ConfigSection *sect);

#endif
