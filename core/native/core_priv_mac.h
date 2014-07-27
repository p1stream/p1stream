#ifndef p1_core_priv_mac_h
#define p1_core_priv_mac_h

#include "core_priv.h"

namespace p1_core {

void module_platform_init();

class video_mixer_mac : public video_mixer_base {
public:
    CGLContextObj cglContext;
    IOSurfaceRef surface;

    virtual bool platform_init(Handle<Object> params) final;
    virtual void platform_destroy() final;
    virtual bool activate_gl() final;
};


}  // namespace p1_core

#endif  // p1_core_priv_mac_h
