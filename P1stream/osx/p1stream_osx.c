#include "p1stream_priv.h"


void p1_video_frame_iosurface(P1VideoSource *vsrc, IOSurfaceRef buffer)
{
    P1Source *src = (P1Source *) vsrc;
    P1ContextFull *ctx = (P1ContextFull *) src->ctx;

    GLsizei width = (GLsizei) IOSurfaceGetWidth(buffer);
    GLsizei height = (GLsizei) IOSurfaceGetHeight(buffer);
    CGLError err = CGLTexImageIOSurface2D(
                                          ctx->gl, GL_TEXTURE_RECTANGLE,
                                          GL_RGBA8, width, height,
                                          GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, buffer, 0);
    assert(err == kCGLNoError);
}
