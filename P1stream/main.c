#include <stdio.h>
#include <CoreFoundation/CoreFoundation.h>

#include "p1stream.h"


int main(int argc, const char * argv[])
{
    if (argc != 2) {
        printf("Usage: %s <config.plist>\n", argv[0]);
        return 2;
    }

    P1Config *cfg = p1_conf_plist_from_file(argv[1]);
    P1Context *ctx = p1_create(cfg);

    P1VideoClock *video_clock = p1_display_video_clock_create();
    p1_video_set_clock(ctx, video_clock);
    video_clock->start(video_clock);

    P1VideoSource *video_source = p1_display_video_source_create();
    p1_video_add_source(ctx, video_source);
    video_source->start(video_source);

    P1AudioSource *audio_source = p1_input_audio_source_create();
    p1_audio_add_source(ctx, audio_source);
    audio_source->start(audio_source);

    CFRunLoopRun();

    return 0;
}
