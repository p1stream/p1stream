#import "P1OpenGLContext.h"


G_DEFINE_TYPE(P1GOpenGLContext, p1g_opengl_context, G_TYPE_OBJECT)
static GObjectClass *parent_class;

static void p1g_opengl_context_dispose(GObject *gobject);

struct _P1GOpenGLSharedContext
{
    P1GOpenGLContext *parent;
    CGLContextObj context;
};


static void p1g_opengl_context_class_init(P1GOpenGLContextClass *klass)
{
    parent_class = g_type_class_ref(G_TYPE_OBJECT);

    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->dispose = p1g_opengl_context_dispose;
}

static void p1g_opengl_context_init(P1GOpenGLContext *self)
{
}

static void p1g_opengl_context_dispose(GObject *gobject)
{
    P1GOpenGLContext *self = P1G_OPENGL_CONTEXT(gobject);

    CGLReleaseContext(self->context);
    if (self->pixel_format)
        CGLReleasePixelFormat(self->pixel_format);
    if (self->parent)
        g_object_unref(self->parent);

    parent_class->dispose(gobject);
}

P1GOpenGLContext *p1g_opengl_context_new(P1GOpenGLContext *parent)
{
    CGLError err;
    CGLPixelFormatObj pixel_format;
    CGLContextObj parent_context, context;

    if (parent) {
        if (parent->parent)
            parent = parent->parent;

        pixel_format = parent->pixel_format;
        parent_context = parent->context;
    }
    else {
        const CGLPixelFormatAttribute attribs[] = {
            kCGLPFAOpenGLProfile, kCGLOGLPVersion_3_2_Core,
            0
        };
        err = CGLChoosePixelFormat(attribs, &pixel_format, NULL);
        g_return_val_if_fail(err == kCGLNoError, NULL);

        parent_context = NULL;
    }

    err = CGLCreateContext(pixel_format, parent_context, &context);
    g_return_val_if_fail(err == kCGLNoError, NULL);

    P1GOpenGLContext *obj = g_object_new(P1G_TYPE_OPENGL_CONTEXT, NULL);
    obj->context = context;
    if (parent) {
        obj->parent = g_object_ref(parent);
        obj->pixel_format = NULL;
    }
    else {
        obj->parent = NULL;
        obj->pixel_format = pixel_format;
    }
    return obj;
}

P1GOpenGLContext *p1g_opengl_context_new_existing(CGLContextObj ctx)
{
    P1GOpenGLContext *obj = g_object_new(P1G_TYPE_OPENGL_CONTEXT, NULL);
    obj->parent = NULL;
    obj->pixel_format = CGLRetainPixelFormat(CGLGetPixelFormat(ctx));
    obj->context = CGLRetainContext(ctx);
    return obj;
}
