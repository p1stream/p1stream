#include <stdio.h>
#include <dispatch/dispatch.h>

#include "output.h"
#include "stream.h"
#include "capture_desktop.h"
#include "audio_input.h"


int main(int argc, const char * argv[])
{
    if (argc != 2) {
        printf("Usage: %s <url>\n", argv[0]);
        return 2;
    }

    p1_output_init();
    p1_stream_init(argv[1]);
    p1_capture_desktop_start();
    p1_audio_input_init();
    dispatch_main();

    return 0;
}
