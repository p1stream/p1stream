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
    self->contexts = g_hash_table_new(g_direct_hash, g_direct_equal);
}

static void p1g_opengl_context_dispose(GObject *gobject)
{
    P1GOpenGLContext *self = P1G_OPENGL_CONTEXT(gobject);

    GHashTableIter iter;
    gpointer key, value;

    g_hash_table_iter_init(&iter, self->contexts);
    while (g_hash_table_iter_next(&iter, &key, &value))
        CGLReleaseContext(value);

    CGLReleaseContext(self->main_context);
    g_hash_table_unref(self->contexts);

    parent_class->dispose(gobject);
}

CGLContextObj p1g_opengl_context_activate(P1GOpenGLContext *self)
{
    CGLError err;

    GThread *this_thread = g_thread_self();
    CGLContextObj ctx = g_hash_table_lookup(self->contexts, this_thread);

    if (!ctx) {
        err = CGLCreateContext(self->pixel_format, self->main_context, &ctx);
        g_return_val_if_fail(err == kCGLNoError, NULL);

        g_hash_table_insert(self->contexts, this_thread, ctx);
    }

    err = CGLSetCurrentContext(ctx);
    g_return_val_if_fail(err == kCGLNoError, NULL);

    return ctx;
}

P1GOpenGLContext *p1g_opengl_context_new(CGLContextObj ctx)
{
    CGLError err;
    CGLPixelFormatObj pixel_format;

    if (ctx) {
        CGLRetainContext(ctx);
        pixel_format = CGLGetPixelFormat(ctx);
    }
    else {
        const CGLPixelFormatAttribute attribs[] = {
            kCGLPFAOpenGLProfile, kCGLOGLPVersion_3_2_Core,
            0
        };
        err = CGLChoosePixelFormat(attribs, &pixel_format, NULL);
        g_return_val_if_fail(err == kCGLNoError, NULL);

        err = CGLCreateContext(pixel_format, NULL, &ctx);
        g_return_val_if_fail(err == kCGLNoError, NULL);
    }

    P1GOpenGLContext *obj = g_object_new(P1G_TYPE_OPENGL_CONTEXT, NULL);
    obj->pixel_format = pixel_format;
    obj->main_context = ctx;
    g_hash_table_insert(obj->contexts, g_thread_self(), CGLRetainContext(ctx));

    return obj;
}
