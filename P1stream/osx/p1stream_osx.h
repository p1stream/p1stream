#ifndef p1stream_osx_h
#define p1stream_osx_h

#include <IOSurface/IOSurface.h>

#include "p1stream.h"


// Fast path for OS X video sources that can provide an IOSurface.
void p1_video_frame_iosurface(P1VideoSource *src, IOSurfaceRef buffer);

// Configuration backed by a property list file. Uses Core Foundation.
P1Config *p1_conf_plist_from_file(const char *file);


// Audio source using a system audio input. Uses Audio Toolbox.
P1AudioSource *p1_input_audio_source_create(P1Config *cfg, P1ConfigSection *sect);

// Video clock timed to the refresh rate of a display. Uses Core Video.
P1VideoClock *p1_display_video_clock_create(P1Config *cfg, P1ConfigSection *sect);

// Video source that captures from a display. Uses Core Graphics.
P1VideoSource *p1_display_video_source_create(P1Config *cfg, P1ConfigSection *sect);

// Video source that opens a system capture device. Uses AV Foundation.
P1VideoSource *p1_capture_video_source_create(P1Config *cfg, P1ConfigSection *sect);

#endif
