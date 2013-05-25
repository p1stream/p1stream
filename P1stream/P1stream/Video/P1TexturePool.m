#import "P1TexturePool.h"
#import "P1TextureMeta.h"


G_DEFINE_TYPE(P1TexturePool, p1_texture_pool, GST_TYPE_BUFFER_POOL)
static GstBufferPoolClass *parent_class;

static void p1_texture_pool_dispose(
    GObject *gobject);
static void p1_texture_pool_set_context(
                                        P1TexturePool *self, P1GLContext *context);
static gboolean p1_texture_pool_set_config(
    GstBufferPool *bufferpool, GstStructure *config);
static GstFlowReturn p1_texture_pool_alloc_buffer(
    GstBufferPool *bufferpool, GstBuffer **buffer, GstBufferPoolAcquireParams *params);


static void p1_texture_pool_class_init(P1TexturePoolClass *klass)
{
    parent_class = g_type_class_ref(GST_TYPE_BUFFER_POOL);

    GstBufferPoolClass *bufferpool_class = GST_BUFFER_POOL_CLASS(klass);
    bufferpool_class->set_config   = p1_texture_pool_set_config;
    bufferpool_class->alloc_buffer = p1_texture_pool_alloc_buffer;

    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->dispose = p1_texture_pool_dispose;
}

static void p1_texture_pool_init(P1TexturePool *self)
{
    self->context = NULL;
    self->width = self->height = -1;
}

static void p1_texture_pool_dispose(GObject *gobject)
{
    P1TexturePool *self = P1_TEXTURE_POOL(gobject);

    p1_texture_pool_set_context(self, NULL);

    G_OBJECT_CLASS(parent_class)->dispose(gobject);
}

static void p1_texture_pool_set_context(P1TexturePool *self, P1GLContext *context)
{
    if (self->context != NULL) {
        g_object_unref(self->context);
        self->context = NULL;
    }

    if (context != NULL) {
        self->context = context;
    }
}

static gboolean p1_texture_pool_set_config(
    GstBufferPool *bufferpool, GstStructure *config)
{
    P1TexturePool *self = P1_TEXTURE_POOL(bufferpool);

    // Get at the caps structure (which should be fixated at this point)
    GstCaps *caps;
    if (!gst_buffer_pool_config_get_params(config, &caps, NULL, NULL, NULL))
        return FALSE;
    GstStructure *structure = gst_caps_get_structure(caps, 0);
    if (!structure)
        return FALSE;

    // Extract values
    // FIXME: internal format in caps
    const GValue *context_value = gst_structure_get_value(structure, "context");
    if (context_value == NULL)
        return FALSE;
    if (!gst_structure_get_int(structure, "width", &self->width))
        return FALSE;
    if (!gst_structure_get_int(structure, "height", &self->height))
        return FALSE;

    // Set context
    p1_texture_pool_set_context(self, g_value_dup_object(context_value));

    return parent_class->set_config(bufferpool, config);
}

static GstFlowReturn p1_texture_pool_alloc_buffer(
    GstBufferPool *bufferpool, GstBuffer **outbuf, GstBufferPoolAcquireParams *params)
{
    P1TexturePool *self = P1_TEXTURE_POOL(bufferpool);

    // Allocate the texture
    GstBuffer *buf = gst_buffer_new_texture(self->context);
    g_return_val_if_fail(buf != NULL, GST_FLOW_ERROR);

    // Initialize to the configured size
#if 0
    // FIXME: need to negotiate IOSurface, because CGLTexImageIOSurface2D doesn't
    // like it when we use an initialized texture.
    P1TextureMeta *meta = gst_buffer_get_texture_meta(buf);
    p1_gl_context_lock(meta->context);
    glBindTexture(GL_TEXTURE_RECTANGLE, meta->name);
    glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGBA, self->width, self->height, 0,
                 GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
    p1_gl_context_unlock(meta->context);
#endif

    *outbuf = buf;
    return GST_FLOW_OK;
}

P1TexturePool *p1_texture_pool_new()
{
    return g_object_new(P1_TYPE_TEXTURE_POOL, NULL);
}
