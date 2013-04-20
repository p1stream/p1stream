#import "P1TextureMeta.h"


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

static gboolean p1g_texture_meta_init(GstMeta *meta, gpointer data, GstBuffer *buffer)
{
    P1GTextureMeta *self = (P1GTextureMeta *)meta;
    P1GOpenGLContext *context = data;

    p1g_opengl_context_lock(context);

    glGenTextures(1, &self->name);
    GLenum err = glGetError();

    p1g_opengl_context_unlock(context);
    g_return_val_if_fail(err == GL_NO_ERROR, FALSE);

    self->context = g_object_ref(context);
    self->dependency = NULL;
    return TRUE;
}

static void p1g_texture_meta_free(GstMeta *meta, GstBuffer *buffer)
{
    P1GTextureMeta *self = (P1GTextureMeta *)meta;

    p1g_opengl_context_lock(self->context);

    glDeleteTextures(1, &self->name);
    self->name = 0;

    GLenum err = glGetError();
    p1g_opengl_context_unlock(self->context);

    if (self->dependency) {
        gst_buffer_unref(self->dependency);
        self->dependency = NULL;
    }

    g_object_unref(self->context);

    g_assert(err == GL_NO_ERROR);
}

static gboolean p1g_texture_meta_transform(
    GstBuffer *transbuf, GstMeta *meta, GstBuffer *buffer, GQuark type, gpointer data)
{
    P1GTextureMeta *self = (P1GTextureMeta *)meta;

    P1GTextureMeta *copy = gst_buffer_add_texture_meta(transbuf, self->context);
    // FIXME: Copy state? No expectations here, currently.

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
            p1g_texture_meta_init,
            p1g_texture_meta_free,
            p1g_texture_meta_transform);
        g_once_init_leave(&info, _info);
    }
    return info;
}

GstBuffer *gst_buffer_new_texture(P1GOpenGLContext *context)
{
    GstBuffer *buf = gst_buffer_new();
    P1GTextureMeta *meta = gst_buffer_add_texture_meta(buf, context);
    g_return_val_if_fail(meta != NULL, NULL);
    return buf;
}
