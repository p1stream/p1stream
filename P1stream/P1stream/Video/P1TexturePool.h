#import "P1OpenGLContext.h"


#define P1G_TYPE_TEXTURE_POOL \
    (p1g_texture_pool_get_type())
#define P1G_TEXTURE_POOL_CAST(obj) \
    ((P1GTexturePool *)(obj))
#define P1G_TEXTURE_POOL(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),  P1G_TYPE_TEXTURE_POOL, P1GTexturePool))
#define P1G_TEXTURE_POOL_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),   P1G_TYPE_TEXTURE_POOL, P1GTexturePoolClass))
#define P1G_IS_TEXTURE_POOL(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),  P1G_TYPE_TEXTURE_POOL))
#define P1G_IS_TEXTURE_POOL_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),   P1G_TYPE_TEXTURE_POOL))
#define P1G_TEXTURE_POOL_GET_CLASS(klass) \
    (G_TYPE_INSTANCE_GET_CLASS((klass), P1G_TYPE_TEXTURE_POOL, P1GTexturePoolClass))

typedef struct _P1GTexturePool P1GTexturePool;
typedef struct _P1GTexturePoolClass P1GTexturePoolClass;

struct _P1GTexturePool
{
    GstBufferPool parent_instance;

    /*< private >*/
    P1GOpenGLContext *context;
};

struct _P1GTexturePoolClass
{
    GstBufferPoolClass parent_class;
};

GType p1g_texture_pool_get_type();

P1GTexturePool *p1g_texture_pool_new(P1GOpenGLContext *ctx);

#define p1g_texture_pool_get_context(self) \
    gst_object_ref(P1G_TEXTURE_POOL_CAST(self)->context)
