#define P1G_TYPE_OPENGL_CONTEXT \
    (p1g_opengl_context_get_type())
#define P1G_OPENGL_CONTEXT_CAST(obj) \
    ((P1GOpenGLContext *)(obj))
#define P1G_OPENGL_CONTEXT(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),  P1G_TYPE_OPENGL_CONTEXT, P1GOpenGLContext))
#define P1G_OPENGL_CONTEXT_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),   P1G_TYPE_OPENGL_CONTEXT, P1GOpenGLContextClass))
#define P1G_IS_OPENGL_CONTEXT(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),  P1G_TYPE_OPENGL_CONTEXT))
#define P1G_IS_OPENGL_CONTEXT_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),   P1G_TYPE_OPENGL_CONTEXT))
#define P1G_OPENGL_CONTEXT_GET_CLASS(klass) \
    (G_TYPE_INSTANCE_GET_CLASS((klass), P1G_TYPE_OPENGL_CONTEXT, P1GOpenGLContextClass))

typedef struct _P1GOpenGLContext P1GOpenGLContext;
typedef struct _P1GOpenGLContextClass P1GOpenGLContextClass;

struct _P1GOpenGLContext
{
    GObject parent_instance;

    /*< private >*/
    P1GOpenGLContext *parent;
    CGLPixelFormatObj pixel_format;
    CGLContextObj context;
};

struct _P1GOpenGLContextClass
{
    GObjectClass parent_class;
};

GType p1g_opengl_context_get_type();


P1GOpenGLContext *p1g_opengl_context_new(P1GOpenGLContext *parent);
P1GOpenGLContext *p1g_opengl_context_new_existing(CGLContextObj ctx);

#define p1g_opengl_context_get_parent(self) \
    g_object_ref(P1G_OPENGL_CONTEXT_CAST(self)->parent)

#define p1g_opengl_context_is_shared_with(self, other) \
    ((P1G_OPENGL_CONTEXT_CAST(self)->parent || (self)) == (P1G_OPENGL_CONTEXT_CAST(other)->parent || (other)))

#define p1g_opengl_context_activate(self) \
    CGLSetCurrentContext(P1G_OPENGL_CONTEXT_CAST(self)->context)
