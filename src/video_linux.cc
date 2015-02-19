#include "p1stream_priv_linux.h"

#include <string.h>

namespace p1stream {


bool video_mixer_linux::platform_init(Handle<Object> params)
{
    bool ok;
    EGLBoolean egl_ret;
    EGLConfig egl_config;
    cl_int cl_err;

    // FIXME: Multiple video mixers will conflict.
    // We need to manage displays.
    egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (!(ok = (egl_display != EGL_NO_DISPLAY)))
        strcpy(last_error, "No EGL display");

    if (ok) {
        egl_ret = eglInitialize(egl_display, NULL, NULL);
        if (!(ok = (egl_ret == EGL_TRUE)))
            sprintf(last_error, "eglInitialize error %d", eglGetError());
    }

    if (ok) {
        egl_ret = eglBindAPI(EGL_OPENGL_API);
        if (!(ok = (egl_ret == EGL_TRUE)))
            sprintf(last_error, "eglBindApi error %d", eglGetError());
    }

    if (ok) {
        EGLint attribs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_NONE
        };
        EGLint num_config;
        egl_ret = eglChooseConfig(egl_display, attribs, &egl_config, 1, &num_config);
        if (!(ok = (egl_ret == EGL_TRUE)))
            sprintf(last_error, "eglChooseConfig error %d", eglGetError());
        else if (!(ok = (num_config > 0)))
            strcpy(last_error, "eglChooseConfig returned no suitable configurations");
    }

    if (ok) {
        EGLint attribs[] = {
            EGL_CONTEXT_MAJOR_VERSION_KHR, 3,
            EGL_CONTEXT_MINOR_VERSION_KHR, 2,
            EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
            EGL_NONE
        };
        egl_context = eglCreateContext(egl_display,
            egl_config, EGL_NO_CONTEXT, attribs);
        if (!(ok = (egl_context != EGL_NO_CONTEXT)))
            sprintf(last_error, "eglCreateContext error %d", eglGetError());
    }

    if (ok) {
        cl_context_properties props[] = {
            CL_EGL_DISPLAY_KHR, (cl_context_properties) egl_display,
            CL_GL_CONTEXT_KHR, (cl_context_properties) egl_context,
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
        glTexImage2D(GL_TEXTURE_RECTANGLE, 0,
            GL_RGBA8, out_dimensions.width, out_dimensions.height, 0,
            GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
        if (!(ok = ((gl_err = glGetError()) == GL_NO_ERROR)))
            sprintf(last_error, "OpenGL error %d", gl_err);
    }

    return ok;
}

void video_mixer_linux::platform_destroy()
{
    EGLBoolean egl_ret;

    if (egl_context != EGL_NO_CONTEXT) {
        egl_ret = eglDestroyContext(egl_display, egl_context);
        if (egl_ret == EGL_FALSE)
            fprintf(stderr, "eglDestroyContext error %d", eglGetError());
        egl_context = EGL_NO_CONTEXT;
    }

    if (egl_display != EGL_NO_DISPLAY) {
        egl_ret = eglTerminate(egl_display);
        if (egl_ret == EGL_FALSE)
            fprintf(stderr, "eglTerminate error %d", eglGetError());
        egl_display = EGL_NO_DISPLAY;
    }
}

bool video_mixer_linux::activate_gl()
{
    bool ok;
    EGLBoolean egl_ret;

    egl_ret = eglMakeCurrent(egl_display,
        EGL_NO_SURFACE, EGL_NO_SURFACE, egl_context);
    if (!(ok = egl_ret == EGL_TRUE))
        sprintf(last_error, "eglMakeCurrent error %d", eglGetError());

    return ok;
}


}  // namespace p1stream
