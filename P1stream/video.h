#ifndef p1_cli_video_h
#define p1_cli_video_h

#include <IOSurface/IOSurface.h>

void p1_video_init();
void p1_video_frame_idle();
void p1_video_frame_blank();
void p1_video_frame_iosurface(IOSurfaceRef buffer);

#endif
