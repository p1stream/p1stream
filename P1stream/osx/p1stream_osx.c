#include "p1stream_priv.h"


void p1_video_source_frame_iosurface(P1VideoSource *vsrc, IOSurfaceRef buffer)
{
    P1Object *el = (P1Object *) vsrc;
    P1Video *video = el->ctx->video;
    P1VideoFull *videof = (P1VideoFull *) video;

    GLsizei width = (GLsizei) IOSurfaceGetWidth(buffer);
    GLsizei height = (GLsizei) IOSurfaceGetHeight(buffer);
    CGLError err = CGLTexImageIOSurface2D(
        videof->gl, GL_TEXTURE_RECTANGLE,
        GL_RGBA8, width, height,
        GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, buffer, 0);
    assert(err == kCGLNoError);
}
