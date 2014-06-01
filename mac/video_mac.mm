#include "video_mac.h"

#include <Foundation/Foundation.h>

namespace p1stream {


Handle<Value> video_mixer_mac::platform_init(Handle<Object> params)
{
    bool ok;
    CGLError cgl_err;
    cl_int cl_err;
    GLenum gl_err;

    CGLPixelFormatObj pixel_format;
    const CGLPixelFormatAttribute attribs[] = {
        kCGLPFAOpenGLProfile, (CGLPixelFormatAttribute) kCGLOGLPVersion_3_2_Core,
        kCGLPFAAcceleratedCompute,
        (CGLPixelFormatAttribute) 0
    };
    GLint npix;
    cgl_err = CGLChoosePixelFormat(attribs, &pixel_format, &npix);
    if (!(ok = (cgl_err == kCGLNoError)))
        sprintf(last_error, "CGLChoosePixelFormat error %d", cgl_err);

    if (ok) {
        cgl_err = CGLCreateContext(pixel_format, NULL, &cglContext);
        CGLReleasePixelFormat(pixel_format);
        if (!(ok = (cgl_err == kCGLNoError)))
            sprintf(last_error, "CGLCreateContext error %d", cgl_err);
    }

    if (ok) {
        CGLShareGroupObj share_group = CGLGetShareGroup(cglContext);
        cl_context_properties props[] = {
            CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE, (cl_context_properties) share_group,
            0
        };
        cl = clCreateContext(props, 0, NULL, NULL, NULL, &cl_err);
        if (!(ok = (cl_err == CL_SUCCESS)))
            sprintf(last_error, "clCreateContext error %d", cl_err);
    }

    if (ok) {
        @autoreleasepool {
            surface = IOSurfaceCreate((__bridge CFDictionaryRef) @{
                (__bridge NSString *) kIOSurfaceWidth:  [NSNumber numberWithInt:out_dimensions.width],
                (__bridge NSString *) kIOSurfaceHeight: [NSNumber numberWithInt:out_dimensions.height],
                (__bridge NSString *) kIOSurfaceBytesPerElement: @4
            });
        }
        if (!(ok = (surface != nullptr)))
            strcpy(last_error, "IOSurfaceCreate error");
    }

    if (ok)
        ok = activate_gl();

    if (ok) {
        glGenTextures(1, &tex);
        glGenFramebuffers(1, &fbo);
        glBindTexture(GL_TEXTURE_RECTANGLE, tex);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        if (!(ok = ((gl_err = glGetError()) == GL_NO_ERROR)))
            sprintf(last_error, "OpenGL error %d", gl_err);
    }

    if (ok) {
        cgl_err = CGLTexImageIOSurface2D(cglContext, GL_TEXTURE_RECTANGLE,
            GL_RGBA8, out_dimensions.width, out_dimensions.height,
            GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, surface, 0);
        if (!(ok = (cgl_err == kCGLNoError)))
            sprintf(last_error, "CGLTexImageIOSurface2D error %d", cgl_err);
    }

    if (ok) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, tex, 0);
        if (!(ok = ((gl_err = glGetError()) == GL_NO_ERROR)))
            sprintf(last_error, "glFramebufferTexture2D error %d", gl_err);
    }

    return ok ? Handle<Value>() : pop_last_error();
}

void video_mixer_mac::platform_destroy()
{
    if (surface != nullptr) {
        CFRelease(surface);
        surface = nullptr;
    }

    if (cl != nullptr) {
        cl_int cl_err = clReleaseContext(cl);
        if (cl_err != CL_SUCCESS)
            fprintf(stderr, "clReleaseContext error %d", cl_err);
        cl = nullptr;
    }

    if (cglContext) {
        CGLReleaseContext(cglContext);
        cglContext = nullptr;
    }
}

bool video_mixer_mac::activate_gl()
{
    CGLError cgl_err = CGLSetCurrentContext(cglContext);
    if (cgl_err != kCGLNoError) {
        sprintf(last_error, "CGLSetCurrentContext error %d", cgl_err);
        return false;
    }
    return true;
}

void video_mixer_mac::render_iosurface(IOSurfaceRef surface)
{
    GLsizei width = (GLsizei) IOSurfaceGetWidth(surface);
    GLsizei height = (GLsizei) IOSurfaceGetHeight(surface);
    CGLError err = CGLTexImageIOSurface2D(
        cglContext, GL_TEXTURE_RECTANGLE,
        GL_RGBA8, width, height,
        GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, surface, 0);
    if (err != kCGLNoError)
        sprintf(last_error, "CGLTexImageIOSurface2D error %d", err);
    else
        render_texture();
}


}  // namespace p1stream
