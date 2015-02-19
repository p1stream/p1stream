#include "p1stream_priv_mac.h"

#include <CoreFoundation/CoreFoundation.h>

namespace p1stream {

static const int surface_bytes_per_element = 4;

bool video_mixer_mac::platform_init(Handle<Object> params)
{
    bool ok;
    CGLError cgl_err;
    cl_int cl_err;
    GLenum gl_err;

    // Cast these to ensure we match CFNumber types.
    const long surface_width = out_dimensions.width;
    const long surface_height = out_dimensions.height;

    const CFTypeRef surf_attr_keys[] = {
        kIOSurfaceWidth,
        kIOSurfaceHeight,
        kIOSurfaceBytesPerElement
    };
    const CFTypeRef surf_attr_values[] = {
        CFNumberCreate(NULL, kCFNumberLongType, &surface_width),
        CFNumberCreate(NULL, kCFNumberLongType, &surface_height),
        CFNumberCreate(NULL, kCFNumberIntType, &surface_bytes_per_element)
    };
    if (!(ok = (
        surf_attr_values[0] != NULL &&
        surf_attr_values[1] != NULL &&
        surf_attr_values[2] != NULL
    )))
        sprintf(last_error, "CFNumberCreate error");

    CFDictionaryRef surf_attr;
    if (ok) {
        surf_attr = CFDictionaryCreate(NULL,
            (const void **) surf_attr_keys, (const void **) surf_attr_values,
            3, NULL, &kCFTypeDictionaryValueCallBacks
        );
        if (!(ok = (surf_attr != NULL)))
            sprintf(last_error, "CFDictionaryCreate error");
    }

    if (surf_attr_values[0] != NULL)
        CFRelease(surf_attr_values[0]);
    if (surf_attr_values[1] != NULL)
        CFRelease(surf_attr_values[0]);
    if (surf_attr_values[2] != NULL)
        CFRelease(surf_attr_values[0]);

    if (ok) {
        surface_ = IOSurfaceCreate(surf_attr);
        CFRelease(surf_attr);
        if (!(ok = (surface_ != NULL)))
            sprintf(last_error, "IOSurfaceCreate error");
    }

    CGLPixelFormatObj pixel_format;
    if (ok) {
        const CGLPixelFormatAttribute attribs[] = {
            kCGLPFAOpenGLProfile, (CGLPixelFormatAttribute) kCGLOGLPVersion_3_2_Core,
            kCGLPFAAcceleratedCompute,
            (CGLPixelFormatAttribute) 0
        };
        GLint npix;
        cgl_err = CGLChoosePixelFormat(attribs, &pixel_format, &npix);
        if (!(ok = (cgl_err == kCGLNoError)))
            sprintf(last_error, "CGLChoosePixelFormat error %d", cgl_err);
    }

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

    if (ok) {
        ok = activate_gl();
    }

    if (ok) {
        glGenTextures(1, &texture_);
        glBindTexture(GL_TEXTURE_RECTANGLE, texture_);
        if (!(ok = ((gl_err = glGetError()) == GL_NO_ERROR)))
            sprintf(last_error, "OpenGL error %d", gl_err);
    }

    if (ok) {
        CGLError err = CGLTexImageIOSurface2D(
            cgl_context, GL_TEXTURE_RECTANGLE,
            GL_RGBA8, surface_width, surface_height,
            GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, surface_, 0);
        if (err != kCGLNoError)
            sprintf(last_error, "CGLTexImageIOSurface2D error %d", err);
    }

    return ok;
}

void video_mixer_mac::platform_destroy()
{
    if (cgl_context) {
        CGLReleaseContext(cgl_context);
        cgl_context = nullptr;
    }

    if (surface_) {
        CFRelease(surface_);
        surface_ = NULL;
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


}  // namespace p1stream
