#import "P1FrameBufferMeta.h"


GType p1g_frame_buffer_meta_api_get_type()
{
    static volatile GType type;
    if (g_once_init_enter(&type)) {
        const gchar *tags[] = { "memory", NULL };
        GType _type = gst_meta_api_type_register("P1GFrameBufferMetaAPI", tags);
        g_once_init_leave(&type, _type);
    }
    return type;
}

static gboolean p1g_frame_buffer_meta_init(GstMeta *meta, gpointer data, GstBuffer *buffer)
{
    P1GFrameBufferMeta *self = (P1GFrameBufferMeta *)meta;
    P1GOpenGLContext *context = data;

    p1g_opengl_context_lock(context);

    glGenFramebuffers(1, &self->name);
    GLenum err = glGetError();

    p1g_opengl_context_unlock(context);
    g_return_val_if_fail(err == GL_NO_ERROR, FALSE);

    self->context = g_object_ref(context);
    return TRUE;
}

static void p1g_frame_buffer_meta_free(GstMeta *meta, GstBuffer *buffer)
{
    P1GFrameBufferMeta *self = (P1GFrameBufferMeta *)meta;

    p1g_opengl_context_lock(self->context);

    glDeleteFramebuffers(1, &self->name);
    self->name = 0;

    g_assert(glGetError() == GL_NO_ERROR);
    p1g_opengl_context_unlock(self->context);

    g_object_unref(self->context);
}

static gboolean p1g_frame_buffer_meta_transform(
    GstBuffer *transbuf, GstMeta *meta, GstBuffer *buffer, GQuark type, gpointer data)
{
    P1GFrameBufferMeta *self = (P1GFrameBufferMeta *)meta;

    P1GFrameBufferMeta *copy = gst_buffer_add_frame_buffer_meta(transbuf, self->context);
    // FIXME: Copy state? No expectations here, currently.

    return copy != NULL;
}

const GstMetaInfo *p1g_frame_buffer_meta_get_info()
{
    static const GstMetaInfo *info = NULL;
    if (g_once_init_enter(&info)) {
        const GstMetaInfo *_info = gst_meta_register(
            P1G_FRAME_BUFFER_META_API_TYPE,
            "P1GFrameBufferMeta",
            sizeof(P1GFrameBufferMeta),
            p1g_frame_buffer_meta_init,
            p1g_frame_buffer_meta_free,
            p1g_frame_buffer_meta_transform);
        g_once_init_leave(&info, _info);
    }
    return info;
}

GstBuffer *gst_buffer_new_frame_buffer(P1GOpenGLContext *context)
{
    GstBuffer *buf = gst_buffer_new();
    P1GFrameBufferMeta *meta = gst_buffer_add_frame_buffer_meta(buf, context);
    g_return_val_if_fail(meta != NULL, NULL);
    return buf;
}
