#include "p1stream_priv.h"


void p1_log_ns_string(P1Object *obj, P1LogLevel level, NSString *str)
{
    p1_log(obj, level, "%s", [str UTF8String]);
}

void p1_log_ns_error(P1Object *obj, P1LogLevel level, NSError *err)
{
    while (err != nil) {
        p1_log_ns_string(obj, level, [err localizedFailureReason]);
        err = [err.userInfo objectForKey:NSUnderlyingErrorKey];
    }
}

void p1_log_cf_error(P1Object *obj, P1LogLevel level, CFErrorRef err)
{
    p1_log_ns_error(obj, level, (__bridge NSError *) err);
}

void p1_log_os_status(P1Object *obj, P1LogLevel level, OSStatus status)
{
    @autoreleasepool {
        NSError *err = [NSError errorWithDomain:NSOSStatusErrorDomain
                                           code:status
                                       userInfo:nil];
        p1_log_ns_error(obj, level, err);
    }
}


bool p1_video_source_frame_iosurface(P1VideoSource *vsrc, IOSurfaceRef buffer)
{
    P1Object *obj = (P1Object *) vsrc;
    P1Video *video = obj->ctx->video;
    P1VideoFull *videof = (P1VideoFull *) video;

    GLsizei width = (GLsizei) IOSurfaceGetWidth(buffer);
    GLsizei height = (GLsizei) IOSurfaceGetHeight(buffer);
    CGLError err = CGLTexImageIOSurface2D(
        videof->gl, GL_TEXTURE_RECTANGLE,
        GL_RGBA8, width, height,
        GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, buffer, 0);
    if (err != kCGLNoError) {
        p1_log(obj, P1_LOG_ERROR, "Failed to upload IOSurface: Core Graphics error %d", err);
        return false;
    }

    return true;
}
