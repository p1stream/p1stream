#ifndef p1stream_priv_linux_h
#define p1stream_priv_linux_h

#include "p1stream_priv.h"

#define MESA_EGL_NO_X11_HEADERS
#include <EGL/egl.h>
#include <EGL/eglext.h>

namespace p1stream {


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


}  // namespace p1stream

#endif  // p1stream_priv_linux_h
