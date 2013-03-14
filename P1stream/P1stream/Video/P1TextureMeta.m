#import "P1TextureMeta.h"


#define gst_buffer_add_texture_meta(b, ctx) \
    ((P1GTextureMeta *)gst_buffer_add_meta((b), P1G_TEXTURE_META_INFO, ctx))


GType p1g_texture_meta_api_get_type()
{
    static volatile GType type;
    if (g_once_init_enter(&type)) {
        const gchar *tags[] = { "memory", NULL };
        GType _type = gst_meta_api_type_register("P1GTextureMetaAPI", tags);
        g_once_init_leave(&type, _type);
    }
    return type;
}

static gboolean p1_texture_meta_init(GstMeta *meta, gpointer ctx, GstBuffer *buffer)
{
    P1GTextureMeta *self = (P1GTextureMeta *)meta;

    self->context = g_object_ref(ctx);
    self->texture_name = 0;

    self->dependency = NULL;

    return TRUE;
}

static void p1_texture_meta_free(GstMeta *meta, GstBuffer *buffer)
{
    P1GTextureMeta *self = (P1GTextureMeta *)meta;

    if (self->dependency)
        gst_buffer_unref(self->dependency);

    g_object_unref(self->context);
}

static gboolean p1_texture_meta_transform(
    GstBuffer *transbuf, GstMeta *meta, GstBuffer *buffer, GQuark type, gpointer data)
{
    P1GTextureMeta *self = (P1GTextureMeta *)meta;

    P1GTextureMeta *copy = gst_buffer_add_texture_meta(transbuf, self->context);

    return copy != NULL;
}

const GstMetaInfo *p1g_texture_meta_get_info()
{
    static const GstMetaInfo *info = NULL;
    if (g_once_init_enter(&info)) {
        const GstMetaInfo *_info = gst_meta_register(
            P1G_TEXTURE_META_API_TYPE,
            "P1GTextureMeta",
            sizeof(P1GTextureMeta),
            p1_texture_meta_init,
            p1_texture_meta_free,
            p1_texture_meta_transform);
        g_once_init_leave(&info, _info);
    }
    return info;
}

GstBuffer *gst_buffer_new_texture(P1GOpenGLContext *ctx)
{
    p1g_opengl_context_activate(ctx);

    GstBuffer *buf = gst_buffer_new();

    P1GTextureMeta *meta = gst_buffer_add_texture_meta(buf, ctx);
    glGenTextures(1, &meta->texture_name);
    g_return_val_if_fail(glGetError() == GL_NO_ERROR, NULL);

    return buf;
}
