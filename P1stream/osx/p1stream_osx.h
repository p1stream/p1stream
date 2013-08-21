#ifndef p1stream_osx_h
#define p1stream_osx_h

#include <IOSurface/IOSurface.h>

#include "p1stream.h"


P1Config *p1_conf_plist_from_file(const char *file);

extern P1VideoClockFactory p1_display_video_clock_factory;
extern P1VideoSourceFactory p1_display_video_source_factory;
extern P1AudioSourceFactory p1_input_audio_source_factory;

void p1_video_frame_iosurface(P1VideoSource *src, IOSurfaceRef buffer);

#endif
