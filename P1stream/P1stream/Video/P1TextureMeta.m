#import "P1TextureMeta.h"


GType p1_texture_meta_api_get_type()
{
    static volatile GType type;
    if (g_once_init_enter(&type)) {
        const gchar *tags[] = { "memory", NULL };
        GType _type = gst_meta_api_type_register("P1TextureMetaAPI", tags);
        g_once_init_leave(&type, _type);
    }
    return type;
}

static gboolean p1_texture_meta_init(GstMeta *meta, gpointer data, GstBuffer *buffer)
{
    P1TextureMeta *self = (P1TextureMeta *)meta;
    P1GLContext *context = data;

    p1_gl_context_lock(context);

    glGenTextures(1, &self->name);
    GLenum err = glGetError();

    p1_gl_context_unlock(context);
    g_return_val_if_fail(err == GL_NO_ERROR, FALSE);

    self->context = g_object_ref(context);
    self->dependency = NULL;
    return TRUE;
}

static void p1_texture_meta_free(GstMeta *meta, GstBuffer *buffer)
{
    P1TextureMeta *self = (P1TextureMeta *)meta;

    p1_gl_context_lock(self->context);

    glDeleteTextures(1, &self->name);
    self->name = 0;

    GLenum err = glGetError();
    p1_gl_context_unlock(self->context);

    if (self->dependency) {
        gst_buffer_unref(self->dependency);
        self->dependency = NULL;
    }

    g_object_unref(self->context);

    g_assert(err == GL_NO_ERROR);
}

static gboolean p1_texture_meta_transform(
    GstBuffer *transbuf, GstMeta *meta, GstBuffer *buffer, GQuark type, gpointer data)
{
    P1TextureMeta *self = (P1TextureMeta *)meta;

    P1TextureMeta *copy = gst_buffer_add_texture_meta(transbuf, self->context);
    // FIXME: Copy state? No expectations here, currently.

    return copy != NULL;
}

const GstMetaInfo *p1_texture_meta_get_info()
{
    static const GstMetaInfo *info = NULL;
    if (g_once_init_enter(&info)) {
        const GstMetaInfo *_info = gst_meta_register(
            P1_TEXTURE_META_API_TYPE,
            "P1TextureMeta",
            sizeof(P1TextureMeta),
            p1_texture_meta_init,
            p1_texture_meta_free,
            p1_texture_meta_transform);
        g_once_init_leave(&info, _info);
    }
    return info;
}

GstBuffer *gst_buffer_new_texture(P1GLContext *context)
{
    GstBuffer *buf = gst_buffer_new();
    P1TextureMeta *meta = gst_buffer_add_texture_meta(buf, context);
    g_return_val_if_fail(meta != NULL, NULL);
    return buf;
}
