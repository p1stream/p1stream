#ifndef p1_core_priv_linux_h
#define p1_core_priv_linux_h

#include "core_priv.h"

#define MESA_EGL_NO_X11_HEADERS
#include <EGL/egl.h>
#include <EGL/eglext.h>

namespace p1_core {


void module_platform_init();

class video_mixer_linux : public video_mixer_base {
public:
    video_mixer_linux();

    EGLDisplay egl_display;
    EGLContext egl_context;

    virtual bool platform_init(Handle<Object> params) final;
    virtual void platform_destroy() final;
    virtual bool activate_gl() final;
};


// ----- Inline implementations -----

inline video_mixer_linux::video_mixer_linux() :
    egl_display(), egl_context()
{
}


}  // namespace p1_core

#endif  // p1_core_priv_linux_h
