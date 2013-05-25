#import "P1GLContext.h"


#define P1_TYPE_TEXTURE_POOL \
    (p1_texture_pool_get_type())
#define P1_TEXTURE_POOL_CAST(obj) \
    ((P1TexturePool *)(obj))
#define P1_TEXTURE_POOL(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),  P1_TYPE_TEXTURE_POOL, P1TexturePool))
#define P1_TEXTURE_POOL_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),   P1_TYPE_TEXTURE_POOL, P1TexturePoolClass))
#define P1_IS_TEXTURE_POOL(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),  P1_TYPE_TEXTURE_POOL))
#define P1_IS_TEXTURE_POOL_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),   P1_TYPE_TEXTURE_POOL))
#define P1_TEXTURE_POOL_GET_CLASS(klass) \
    (G_TYPE_INSTANCE_GET_CLASS((klass), P1_TYPE_TEXTURE_POOL, P1TexturePoolClass))

typedef struct _P1TexturePool P1TexturePool;
typedef struct _P1TexturePoolClass P1TexturePoolClass;

struct _P1TexturePool
{
    GstBufferPool parent_instance;

    /*< private >*/
    P1GLContext *context;
    gint width, height;
};

struct _P1TexturePoolClass
{
    GstBufferPoolClass parent_class;
};

GType p1_texture_pool_get_type();

P1TexturePool *p1_texture_pool_new();
