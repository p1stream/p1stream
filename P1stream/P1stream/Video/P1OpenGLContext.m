#import "P1OpenGLContext.h"


G_DEFINE_TYPE(P1GOpenGLContext, p1g_opengl_context, G_TYPE_OBJECT)
static GObjectClass *parent_class;

static void p1g_opengl_context_dispose(GObject *gobject);


static void p1g_opengl_context_class_init(P1GOpenGLContextClass *klass)
{
    parent_class = g_type_class_ref(G_TYPE_OBJECT);

    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->dispose = p1g_opengl_context_dispose;
}

static void p1g_opengl_context_init(P1GOpenGLContext *self)
{
    self->context = NULL;
}

static void p1g_opengl_context_dispose(GObject *gobject)
{
    P1GOpenGLContext *self = P1G_OPENGL_CONTEXT(gobject);

    if (self->context) {
        CGLReleaseContext(self->context);
        self->context = NULL;
    }

    parent_class->dispose(gobject);
}

P1GOpenGLContext *p1g_opengl_context_new()
{
    CGLError err;

    CGLPixelFormatObj pixel_format;
    const CGLPixelFormatAttribute attribs[] = {
        kCGLPFAOpenGLProfile, kCGLOGLPVersion_3_2_Core,
        0
    };
    GLint npix;
    err = CGLChoosePixelFormat(attribs, &pixel_format, &npix);
    g_return_val_if_fail(err == kCGLNoError, NULL);

    CGLContextObj context;
    err = CGLCreateContext(pixel_format, NULL, &context);
    CGLReleasePixelFormat(pixel_format);
    g_return_val_if_fail(err == kCGLNoError, NULL);

    P1GOpenGLContext *obj = g_object_new(P1G_TYPE_OPENGL_CONTEXT, NULL);
    obj->context = context;
    return obj;
}

P1GOpenGLContext *p1g_opengl_context_new_shared(P1GOpenGLContext *other)
{
    CGLError err;

    CGLPixelFormatObj pixel_format = CGLGetPixelFormat(other->context);
    g_return_val_if_fail(pixel_format != NULL, NULL);

    CGLContextObj context;
    err = CGLCreateContext(pixel_format, NULL, &context);
    g_return_val_if_fail(err == kCGLNoError, NULL);

    P1GOpenGLContext *obj = g_object_new(P1G_TYPE_OPENGL_CONTEXT, NULL);
    obj->context = context;
    return obj;
}


P1GOpenGLContext *p1g_opengl_context_new_existing(CGLContextObj context)
{
    P1GOpenGLContext *obj = g_object_new(P1G_TYPE_OPENGL_CONTEXT, NULL);
    obj->context = CGLRetainContext(context);
    return obj;
}
