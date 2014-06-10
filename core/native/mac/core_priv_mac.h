#ifndef p1_core_priv_mac_h
#define p1_core_priv_mac_h

#include "core_priv.h"

namespace p1stream {


class video_mixer_mac : public video_mixer_base {
    CGLContextObj cglContext;
    IOSurfaceRef surface;

protected:
    virtual Handle<Value> platform_init(Handle<Object> params) final;
    virtual void platform_destroy() final;
    virtual bool activate_gl() final;

public:
    virtual void render_iosurface(IOSurfaceRef surface) final;
};


}  // namespace p1stream

#endif  // p1_core_priv_mac_h
