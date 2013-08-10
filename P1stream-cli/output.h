#ifndef P1stream_cli_output_h
#define P1stream_cli_output_h

#include <IOSurface/IOSurface.h>

void p1_output_init();
void p1_output_video_idle();
void p1_output_video_blank();
void p1_output_video_iosurface(IOSurfaceRef buffer);

#endif
