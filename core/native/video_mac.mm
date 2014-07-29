#include "core_priv_mac.h"

#include <Foundation/Foundation.h>

namespace p1_core {


bool video_mixer_mac::platform_init(Handle<Object> params)
{
    bool ok;
    CGLError cgl_err;
    cl_int cl_err;

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
        cgl_err = CGLCreateContext(pixel_format, NULL, &cgl_context);
        CGLReleasePixelFormat(pixel_format);
        if (!(ok = (cgl_err == kCGLNoError)))
            sprintf(last_error, "CGLCreateContext error %d", cgl_err);
    }

    if (ok) {
        CGLShareGroupObj share_group = CGLGetShareGroup(cgl_context);
        cl_context_properties props[] = {
            CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE, (cl_context_properties) share_group,
            0
        };
        cl = clCreateContext(props, 0, NULL, NULL, NULL, &cl_err);
        if (!(ok = (cl_err == CL_SUCCESS)))
            sprintf(last_error, "clCreateContext error %d", cl_err);
    }

    return ok;
}

void video_mixer_mac::platform_destroy()
{
    if (cgl_context) {
        CGLReleaseContext(cgl_context);
        cgl_context = nullptr;
    }
}

bool video_mixer_mac::activate_gl()
{
    CGLError cgl_err = CGLSetCurrentContext(cgl_context);
    if (cgl_err != kCGLNoError) {
        sprintf(last_error, "CGLSetCurrentContext error %d", cgl_err);
        return false;
    }
    return true;
}


void video_source_context::render_iosurface(IOSurfaceRef surface)
{
    auto &mixer_mac = *((video_mixer_mac *) mixer_);

    GLsizei width = (GLsizei) IOSurfaceGetWidth(surface);
    GLsizei height = (GLsizei) IOSurfaceGetHeight(surface);

    CGLError err = CGLTexImageIOSurface2D(
        mixer_mac.cgl_context, GL_TEXTURE_RECTANGLE,
        GL_RGBA8, width, height,
        GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, surface, 0);
    if (err != kCGLNoError)
        sprintf(mixer_mac.last_error, "CGLTexImageIOSurface2D error %d", err);
    else
        render_texture();
}


}  // namespace p1_core
