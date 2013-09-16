#ifndef p1stream_osx_priv_h
#define p1stream_osx_priv_h

#ifndef GL_DO_NOT_WARN_IF_MULTI_GL_VERSION_HEADERS_INCLUDED
#define GL_DO_NOT_WARN_IF_MULTI_GL_VERSION_HEADERS_INCLUDED
#endif

#include <mach/mach_time.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>
#include <OpenCL/opencl.h>

typedef struct _P1GLContext P1GLContext;
typedef struct _P1CLContext P1CLContext;


bool p1_init_platform(P1ContextFull *ctxf);
#define p1_destroy_platform(_ctxf)

#define p1_get_time() mach_absolute_time()


struct _P1GLContext {
    CGLContextObj cglContext;
    IOSurfaceRef surface;
};

bool p1_video_init_platform(P1VideoFull *videof);
void p1_video_destroy_platform(P1VideoFull *videof);

#define p1_video_activate_gl(_videof) ({                                    \
    P1VideoFull *_p1_videof = (P1VideoFull *) (_videof);                    \
    P1Object *_p1_videoobj = (P1Object *) _p1_videof;                       \
    CGLError _p1_ret = CGLSetCurrentContext(_p1_videof->gl.cglContext);     \
    bool _p1_ok = (_p1_ret == kCGLNoError);                                 \
    if (!_p1_ok)                                                            \
        p1_log(_p1_videoobj, P1_LOG_ERROR, "Failed to activate GL context: Core Graphics error %d", _p1_ret);   \
    _p1_ok;                                                                 \
})

bool p1_video_preview(P1VideoFull *videof);

#endif
