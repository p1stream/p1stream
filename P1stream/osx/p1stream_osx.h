#ifndef p1stream_osx_h
#define p1stream_osx_h

#include <IOSurface/IOSurface.h>

#include "p1stream.h"


void p1_video_frame_iosurface(P1VideoSource *src, IOSurfaceRef buffer);

P1Config *p1_conf_plist_from_file(const char *file);
P1AudioSource *p1_input_audio_source_create();
P1VideoClock *p1_display_video_clock_create();
P1VideoSource *p1_display_video_source_create();
P1VideoSource *p1_capture_video_source_create();

#endif
