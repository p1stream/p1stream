#ifndef p1_video_osx_h
#define p1_video_osx_h

#include "video_base.h"

namespace p1stream {

class video_mixer_osx : public video_mixer_base {
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

#endif  // p1_video_osx_h
