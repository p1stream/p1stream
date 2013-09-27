#include "p1stream_priv.h"

#include <mach/mach_error.h>


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


bool p1_init_platform(P1ContextFull *ctxf)
{
    P1Object *ctxobj = (P1Object *) ctxf;
    kern_return_t ret;
    mach_timebase_info_data_t timebase;

    ret = mach_timebase_info(&timebase);
    if (ret != 0) {
        p1_log(ctxobj, P1_LOG_ERROR, "Failed to get mach timebase: %s", mach_error_string(errno));
        return false;
    }

    ctxf->timebase_num = timebase.numer;
    ctxf->timebase_den = timebase.denom;

    return true;
}


bool p1_video_init_platform(P1VideoFull *videof)
{
    P1Object *videoobj = (P1Object *) videof;
    CGLError cgl_err;
    cl_int cl_err;
    GLenum gl_err;

    CGLPixelFormatObj pixel_format;
    const CGLPixelFormatAttribute attribs[] = {
        kCGLPFAOpenGLProfile, (CGLPixelFormatAttribute) kCGLOGLPVersion_3_2_Core,
        0
    };
    GLint npix;
    cgl_err = CGLChoosePixelFormat(attribs, &pixel_format, &npix);
    if (cgl_err != kCGLNoError) {
        p1_log(videoobj, P1_LOG_ERROR, "Failed to choose GL pixel format: Core Graphics error %d", cgl_err);
        goto fail;
    }

    cgl_err = CGLCreateContext(pixel_format, NULL, &videof->gl.cglContext);
    CGLReleasePixelFormat(pixel_format);
    if (cgl_err != kCGLNoError) {
        p1_log(videoobj, P1_LOG_ERROR, "Failed to create GL context: Core Graphics error %d", cgl_err);
        goto fail;
    }

    CGLShareGroupObj share_group = CGLGetShareGroup(videof->gl.cglContext);
    cl_context_properties props[] = {
        CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE, (cl_context_properties) share_group,
        0
    };
    videof->cl = clCreateContext(props, 0, NULL, p1_video_cl_notify_callback, videoobj, &cl_err);
    if (cl_err != CL_SUCCESS) {
        p1_log(videoobj, P1_LOG_ERROR, "Failed to create CL context: OpenCL error %d", cl_err);
        goto fail_gl;
    }

    @autoreleasepool {
        videof->gl.surface = IOSurfaceCreate((__bridge CFDictionaryRef) @{
            (__bridge NSString *) kIOSurfaceWidth: @1280,
            (__bridge NSString *) kIOSurfaceHeight: @720,
            (__bridge NSString *) kIOSurfaceBytesPerElement: @4
        });
    }
    if (videof->gl.surface == NULL) {
        p1_log(videoobj, P1_LOG_ERROR, "Failed to create IOSurface");
        goto fail_cl;
    }

    if (!p1_video_activate_gl(videof))
        goto fail_iosurface;

    glGenTextures(1, &videof->tex);
    glGenFramebuffers(1, &videof->fbo);
    glBindTexture(GL_TEXTURE_RECTANGLE, videof->tex);
    glBindFramebuffer(GL_FRAMEBUFFER, videof->fbo);
    if ((gl_err = glGetError()) != GL_NO_ERROR) {
        p1_log(videoobj, P1_LOG_ERROR, "Failed to create GL objects: OpenGL error %d", gl_err);
        goto fail_iosurface;
    }

    cgl_err = CGLTexImageIOSurface2D(videof->gl.cglContext, GL_TEXTURE_RECTANGLE,
                                     GL_RGBA8, 1280, 720,
                                     GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, videof->gl.surface, 0);
    if (cgl_err != kCGLNoError) {
        p1_log(videoobj, P1_LOG_ERROR, "Failed to bind IOSurface to GL texture: Core Graphics error %d", cgl_err);
        goto fail_iosurface;
    }

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, videof->tex, 0);
    if ((gl_err = glGetError()) != GL_NO_ERROR) {
        p1_log(videoobj, P1_LOG_ERROR, "Failed to create bind GL texture to frame buffer: OpenGL error %d", gl_err);
        goto fail_iosurface;
    }

    return true;

fail_iosurface:
    CFRelease(videof->gl.surface);

fail_cl:
    cl_err = clReleaseContext(videof->cl);
    if (cl_err != CL_SUCCESS)
        p1_log(videoobj, P1_LOG_ERROR, "Failed to release CL context: OpenCL error %d", cl_err);

fail_gl:
    CGLReleaseContext(videof->gl.cglContext);

fail:
    return false;
}

void p1_video_destroy_platform(P1VideoFull *videof)
{
    P1Object *videoobj = (P1Object *) videof;
    cl_int cl_err;

    CFRelease(videof->gl.surface);

    cl_err = clReleaseContext(videof->cl);
    if (cl_err != CL_SUCCESS)
        p1_log(videoobj, P1_LOG_ERROR, "Failed to release CL context: OpenCL error %d", cl_err);

    CGLReleaseContext(videof->gl.cglContext);
}

bool p1_video_preview(P1VideoFull *videof)
{
    P1Video *video = (P1Video *) videof;
    P1Object *videoobj = (P1Object *) videof;
    uint32_t seed;
    IOReturn ret;

    ret = IOSurfaceLock(videof->gl.surface, kIOSurfaceLockReadOnly, &seed);
    if (ret != kIOReturnSuccess) {
        p1_log(videoobj, P1_LOG_DEBUG, "Failed to lock IOSurface: IOKit error %d", ret);
        return false;
    }

    uint8_t *data = IOSurfaceGetBaseAddress(videof->gl.surface);
    video->preview_fn(1280, 720, data, video->preview_user_data);

    ret = IOSurfaceUnlock(videof->gl.surface, kIOSurfaceLockReadOnly, &seed);
    if (ret != kIOReturnSuccess)
        p1_log(videoobj, P1_LOG_DEBUG, "Failed to unlock IOSurface: IOKit error %d", ret);

    return true;
}


bool p1_video_source_frame_iosurface(P1VideoSource *vsrc, IOSurfaceRef buffer)
{
    P1Object *obj = (P1Object *) vsrc;
    P1Video *video = obj->ctx->video;
    P1VideoFull *videof = (P1VideoFull *) video;

    GLsizei width = (GLsizei) IOSurfaceGetWidth(buffer);
    GLsizei height = (GLsizei) IOSurfaceGetHeight(buffer);
    CGLError err = CGLTexImageIOSurface2D(
        videof->gl.cglContext, GL_TEXTURE_RECTANGLE,
        GL_RGBA8, width, height,
        GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, buffer, 0);
    if (err != kCGLNoError) {
        p1_log(obj, P1_LOG_ERROR, "Failed to upload IOSurface: Core Graphics error %d", err);
        return false;
    }

    return true;
}
