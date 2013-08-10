#include <stdio.h>
#include <dispatch/dispatch.h>

#include "output.h"
#include "capture_desktop.h"


int main(int argc, const char * argv[])
{
    p1_output_init();
    p1_capture_desktop_start();
    dispatch_main();
    return 0;
}
