#include <stdio.h>
#include <CoreFoundation/CoreFoundation.h>

#include "conf.h"
#include "audio.h"
#include "video.h"
#include "stream.h"

P1Config *p1_conf_plist_from_file(const char *file);
extern P1VideoClockFactory p1_display_video_clock_factory;
extern P1VideoSourceFactory p1_display_video_source_factory;
extern P1AudioSourceFactory p1_input_audio_source_factory;


int main(int argc, const char * argv[])
{
    if (argc != 2) {
        printf("Usage: %s <config.plist>\n", argv[0]);
        return 2;
    }

    P1Config *cfg = p1_conf_plist_from_file(argv[1]);
    p1_audio_init();
    p1_video_init(cfg);
    p1_stream_init(cfg);

    P1VideoClock *video_clock = p1_display_video_clock_factory.create();
    p1_video_set_clock(video_clock);
    video_clock->start(video_clock);

    P1VideoSource *video_source = p1_display_video_source_factory.create();
    p1_video_add_source(video_source);
    video_source->start(video_source);

    P1AudioSource *audio_source = p1_input_audio_source_factory.create();
    p1_audio_add_source(audio_source);
    audio_source->start(audio_source);

    CFRunLoopRun();

    return 0;
}
