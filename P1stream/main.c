#include <stdio.h>
#include <dispatch/dispatch.h>

#include "conf.h"
#include "video.h"
#include "stream.h"

#include "audio_input.h"
#include "video_desktop.h"


int main(int argc, const char * argv[])
{
    if (argc != 2) {
        printf("Usage: %s <config.plist>\n", argv[0]);
        return 2;
    }

    p1_conf_init(argv[1]);
    p1_video_init();
    p1_stream_init();
    p1_video_desktop_init();
    p1_audio_input_init();
    dispatch_main();

    return 0;
}
