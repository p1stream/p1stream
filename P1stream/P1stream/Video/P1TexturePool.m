#import "P1TexturePool.h"
#import "P1TextureMeta.h"


G_DEFINE_TYPE(P1GTexturePool, p1g_texture_pool, GST_TYPE_BUFFER_POOL)
static GstBufferPoolClass *parent_class;

static gboolean p1g_texture_pool_start(GstBufferPool *bufferpool);
static GstFlowReturn p1g_texture_pool_alloc_buffer(
    GstBufferPool *bufferpool, GstBuffer **buffer, GstBufferPoolAcquireParams *params);
static void p1g_texture_pool_dispose(GObject *gobject);


static void p1g_texture_pool_class_init(P1GTexturePoolClass *klass)
{
    parent_class = g_type_class_ref(GST_TYPE_BUFFER_POOL);

    GstBufferPoolClass *bufferpool_class = GST_BUFFER_POOL_CLASS(klass);
    bufferpool_class->start        = p1g_texture_pool_start;
    bufferpool_class->alloc_buffer = p1g_texture_pool_alloc_buffer;

    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->dispose = p1g_texture_pool_dispose;
}

static void p1g_texture_pool_init(P1GTexturePool *self)
{
    self->context = NULL;
}

static void p1g_texture_pool_dispose(GObject *gobject)
{
    P1GTexturePool *self = P1G_TEXTURE_POOL(gobject);

    if (self->context != NULL)
        g_object_unref(self->context);

    G_OBJECT_CLASS(parent_class)->dispose(gobject);
}

static gboolean p1g_texture_pool_start(GstBufferPool *bufferpool)
{
    P1GTexturePool *self = P1G_TEXTURE_POOL(bufferpool);

    if (self->context == NULL) {
        self->context = p1g_opengl_context_new(NULL);
        g_return_val_if_fail(self->context != NULL, FALSE);
    }

    return TRUE;
}

static GstFlowReturn p1g_texture_pool_alloc_buffer(
    GstBufferPool *bufferpool, GstBuffer **outbuf, GstBufferPoolAcquireParams *params)
{
    P1GTexturePool *self = P1G_TEXTURE_POOL(bufferpool);

    GstBuffer *buf = gst_buffer_new_texture(self->context);
    g_return_val_if_fail(buf != NULL, GST_FLOW_ERROR);

    *outbuf = buf;
    return GST_FLOW_OK;
}

P1GTexturePool *p1g_texture_pool_new(P1GOpenGLContext *context)
{
    P1GTexturePool *pool = g_object_new(P1G_TYPE_TEXTURE_POOL, NULL);

    if (context != NULL)
        pool->context = g_object_ref(context);

    return pool;
}
