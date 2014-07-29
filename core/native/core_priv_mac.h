#ifndef p1_core_priv_mac_h
#define p1_core_priv_mac_h

#include "core_priv.h"

namespace p1_core {


void module_platform_init();

class video_mixer_mac : public video_mixer_base {
public:
    video_mixer_mac();

    CGLContextObj cgl_context;

    virtual bool platform_init(Handle<Object> params) final;
    virtual void platform_destroy() final;
    virtual bool activate_gl() final;
};


// ----- Inline implementations -----

inline video_mixer_mac::video_mixer_mac() :
    cgl_context()
{
}


}  // namespace p1_core

#endif  // p1_core_priv_mac_h
