#include "core_priv_linux.h"

#include <string.h>

namespace p1_core {


bool video_mixer_linux::platform_init(Handle<Object> params)
{
    strcpy(last_error, "Not implemented");
    return false;
}

void video_mixer_linux::platform_destroy()
{
}

bool video_mixer_linux::activate_gl()
{
    strcpy(last_error, "Not implemented");
    return false;
}


}  // namespace p1_core
