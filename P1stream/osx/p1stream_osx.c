#include "p1stream_priv.h"


void p1_video_frame_iosurface(P1VideoSource *src, IOSurfaceRef buffer)
{
    P1Context *ctx = src->ctx;
    assert(src == ctx->video_src);

    GLsizei width = (GLsizei) IOSurfaceGetWidth(buffer);
    GLsizei height = (GLsizei) IOSurfaceGetHeight(buffer);
    CGLError err = CGLTexImageIOSurface2D(
                                          ctx->gl, GL_TEXTURE_RECTANGLE,
                                          GL_RGBA8, width, height,
                                          GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, buffer, 0);
    assert(err == kCGLNoError);
}
