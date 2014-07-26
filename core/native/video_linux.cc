#include "core_priv_linux.h"

namespace p1_core {


Handle<Value> video_mixer_linux::platform_init(Handle<Object> params)
{
    return Handle<Value>();
}

void video_mixer_mac::platform_destroy()
{
}

bool video_mixer_mac::activate_gl()
{
    return false;
}


}  // namespace p1_core
