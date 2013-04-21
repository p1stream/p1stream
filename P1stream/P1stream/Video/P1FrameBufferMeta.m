#import "P1FrameBufferMeta.h"


GType p1_frame_buffer_meta_api_get_type()
{
    static volatile GType type;
    if (g_once_init_enter(&type)) {
        const gchar *tags[] = { "memory", NULL };
        GType _type = gst_meta_api_type_register("P1FrameBufferMetaAPI", tags);
        g_once_init_leave(&type, _type);
    }
    return type;
}

static gboolean p1_frame_buffer_meta_init(GstMeta *meta, gpointer data, GstBuffer *buffer)
{
    P1FrameBufferMeta *self = (P1FrameBufferMeta *)meta;
    P1GLContext *context = data;

    p1_gl_context_lock(context);

    glGenFramebuffers(1, &self->name);
    GLenum err = glGetError();

    p1_gl_context_unlock(context);
    g_return_val_if_fail(err == GL_NO_ERROR, FALSE);

    self->context = g_object_ref(context);
    return TRUE;
}

static void p1_frame_buffer_meta_free(GstMeta *meta, GstBuffer *buffer)
{
    P1FrameBufferMeta *self = (P1FrameBufferMeta *)meta;

    p1_gl_context_lock(self->context);

    glDeleteFramebuffers(1, &self->name);
    self->name = 0;

    g_assert(glGetError() == GL_NO_ERROR);
    p1_gl_context_unlock(self->context);

    g_object_unref(self->context);
}

static gboolean p1_frame_buffer_meta_transform(
    GstBuffer *transbuf, GstMeta *meta, GstBuffer *buffer, GQuark type, gpointer data)
{
    P1FrameBufferMeta *self = (P1FrameBufferMeta *)meta;

    P1FrameBufferMeta *copy = gst_buffer_add_frame_buffer_meta(transbuf, self->context);
    // FIXME: Copy state? No expectations here, currently.

    return copy != NULL;
}

const GstMetaInfo *p1_frame_buffer_meta_get_info()
{
    static const GstMetaInfo *info = NULL;
    if (g_once_init_enter(&info)) {
        const GstMetaInfo *_info = gst_meta_register(
            P1_FRAME_BUFFER_META_API_TYPE,
            "P1FrameBufferMeta",
            sizeof(P1FrameBufferMeta),
            p1_frame_buffer_meta_init,
            p1_frame_buffer_meta_free,
            p1_frame_buffer_meta_transform);
        g_once_init_leave(&info, _info);
    }
    return info;
}

GstBuffer *gst_buffer_new_frame_buffer(P1GLContext *context)
{
    GstBuffer *buf = gst_buffer_new();
    P1FrameBufferMeta *meta = gst_buffer_add_frame_buffer_meta(buf, context);
    g_return_val_if_fail(meta != NULL, NULL);
    return buf;
}
