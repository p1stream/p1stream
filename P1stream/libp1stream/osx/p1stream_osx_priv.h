#ifndef p1stream_osx_priv_h
#define p1stream_osx_priv_h

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
    CGLError _p1_err = CGLSetCurrentContext((_videof)->gl.cglContext);      \
    _p1_err == kCGLNoError;                                                 \
})

bool p1_video_preview(P1VideoFull *videof);

#endif
