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
        buffer.emitf(EV_LOG_ERROR, "CFNumberCreate error");

    CFDictionaryRef surf_attr;
    if (ok) {
        surf_attr = CFDictionaryCreate(NULL,
            (const void **) surf_attr_keys, (const void **) surf_attr_values,
            3, NULL, &kCFTypeDictionaryValueCallBacks
        );
        if (!(ok = (surf_attr != NULL)))
            buffer.emitf(EV_LOG_ERROR, "CFDictionaryCreate error");
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
            buffer.emitf(EV_LOG_ERROR, "IOSurfaceCreate error");
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
            buffer.emitf(EV_LOG_ERROR, "CGLChoosePixelFormat error 0x%x", cgl_err);
    }

    if (ok) {
        cgl_err = CGLCreateContext(pixel_format, NULL, &cgl_context_);
        CGLReleasePixelFormat(pixel_format);
        if (!(ok = (cgl_err == kCGLNoError)))
            buffer.emitf(EV_LOG_ERROR, "CGLCreateContext error 0x%x", cgl_err);
    }

    if (ok) {
        CGLShareGroupObj share_group = CGLGetShareGroup(cgl_context_);
        cl_context_properties props[] = {
            CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE, (cl_context_properties) share_group,
            0
        };
        cl = clCreateContext(props, 0, NULL, NULL, NULL, &cl_err);
        if (!(ok = (cl_err == CL_SUCCESS)))
            buffer.emitf(EV_LOG_ERROR, "clCreateContext error 0x%x", cl_err);
    }

    if (ok) {
        ok = activate_gl();
    }

    if (ok) {
        glGenTextures(1, &texture_);
        glBindTexture(GL_TEXTURE_RECTANGLE, texture_);
        if (!(ok = ((gl_err = glGetError()) == GL_NO_ERROR)))
            buffer.emitf(EV_LOG_ERROR, "OpenGL error 0x%x", gl_err);
    }

    if (ok) {
        CGLError err = CGLTexImageIOSurface2D(
            cgl_context_, GL_TEXTURE_RECTANGLE,
            GL_RGBA8, surface_width, surface_height,
            GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, surface_, 0);
        if (err != kCGLNoError)
            buffer.emitf(EV_LOG_ERROR, "CGLTexImageIOSurface2D error 0x%x", err);
    }

    return ok;
}

void video_mixer_mac::platform_destroy()
{
    if (cgl_context_) {
        CGLReleaseContext(cgl_context_);
        cgl_context_ = nullptr;
    }

    if (surface_) {
        CFRelease(surface_);
        surface_ = NULL;
    }
}

bool video_mixer_mac::activate_gl()
{
    CGLError cgl_err = CGLSetCurrentContext(cgl_context_);
    if (cgl_err != kCGLNoError) {
        buffer.emitf(EV_LOG_ERROR, "CGLSetCurrentContext error 0x%x", cgl_err);
        return false;
    }
    return true;
}


void video_source_context::render_iosurface(IOSurfaceRef surface)
{
    auto &mixer_mac = *((video_mixer_mac *) mixer_);

    GLsizei width = (GLsizei) IOSurfaceGetWidth(surface);
    GLsizei height = (GLsizei) IOSurfaceGetHeight(surface);

    glBindTexture(GL_TEXTURE_RECTANGLE, texture());
    CGLError err = CGLTexImageIOSurface2D(
        mixer_mac.cgl_context(), GL_TEXTURE_RECTANGLE,
        GL_RGBA8, width, height,
        GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, surface, 0);
    if (err != kCGLNoError)
        mixer_mac.buffer.emitf(EV_LOG_ERROR, "CGLTexImageIOSurface2D error 0x%x", err);
    else
        render_texture();
}


}  // namespace p1stream
