#ifndef p1_cli_video_h
#define p1_cli_video_h

#include <IOSurface/IOSurface.h>

void p1_video_init();
void p1_video_frame_idle(int64_t time);
void p1_video_frame_blank(int64_t time);
void p1_video_frame_iosurface(int64_t time, IOSurfaceRef buffer);

#endif
