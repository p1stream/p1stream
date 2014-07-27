#include "core_priv_linux.h"

namespace p1_core {


bool video_mixer_linux::platform_init(Handle<Object> params)
{
    strcpy(last_error, "Not implemented");
    return false;
}

void video_mixer_mac::platform_destroy()
{
}

bool video_mixer_mac::activate_gl()
{
    strcpy(last_error, "Not implemented");
    return false;
}


}  // namespace p1_core
