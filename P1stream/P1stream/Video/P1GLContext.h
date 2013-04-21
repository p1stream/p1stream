#define P1_TYPE_GL_CONTEXT \
    (p1_gl_context_get_type())
#define P1_GL_CONTEXT_CAST(obj) \
    ((P1GLContext *)(obj))
#define P1_GL_CONTEXT(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),  P1_TYPE_GL_CONTEXT, P1GLContext))
#define P1_GL_CONTEXT_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),   P1_TYPE_GL_CONTEXT, P1GLContextClass))
#define P1_IS_GL_CONTEXT(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),  P1_TYPE_GL_CONTEXT))
#define P1_IS_GL_CONTEXT_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),   P1_TYPE_GL_CONTEXT))
#define P1_GL_CONTEXT_GET_CLASS(klass) \
    (G_TYPE_INSTANCE_GET_CLASS((klass), P1_TYPE_GL_CONTEXT, P1GLContextClass))

typedef struct _P1GLContext P1GLContext;
typedef struct _P1GLContextClass P1GLContextClass;

struct _P1GLContext
{
    GObject parent_instance;

    /*< private >*/
    CGLContextObj context;
};

struct _P1GLContextClass
{
    GObjectClass parent_class;
};

GType p1_gl_context_get_type();


#define p1_gl_context_lock(self) do { \
    CGLError __err; \
    const CGLContextObj __ctx = P1_GL_CONTEXT_CAST(self)->context; \
    __err = CGLLockContext(__ctx); g_assert(__err == kCGLNoError); \
    __err = CGLSetCurrentContext(__ctx); g_assert(__err == kCGLNoError); \
} while (0)

#define p1_gl_context_unlock(self) do { \
    CGLError __err; \
    const CGLContextObj __ctx = P1_GL_CONTEXT_CAST(self)->context; \
    __err = CGLUnlockContext(__ctx); g_assert(__err == kCGLNoError); \
} while (0)

#define p1_gl_context_get_raw(self) \
    (P1_GL_CONTEXT_CAST(self)->context)

#define p1_gl_context_is_shared(a, b) \
    (CGLGetShareGroup(a->context) == CGLGetShareGroup(b->context))

P1GLContext *p1_gl_context_new();
P1GLContext *p1_gl_context_new_shared(P1GLContext *other);
P1GLContext *p1_gl_context_new_existing(CGLContextObj context);
