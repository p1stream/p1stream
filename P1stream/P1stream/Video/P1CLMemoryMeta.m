#import "P1CLMemoryMeta.h"


GType p1_cl_memory_meta_api_get_type()
{
    static volatile GType type;
    if (g_once_init_enter(&type)) {
        const gchar *tags[] = { "memory", NULL };
        GType _type = gst_meta_api_type_register("P1CLMemoryMetaAPI", tags);
        g_once_init_leave(&type, _type);
    }
    return type;
}

static void p1_cl_memory_meta_free(GstMeta *meta, GstBuffer *buffer)
{
    P1CLMemoryMeta *self = (P1CLMemoryMeta *)meta;

    p1_cl_context_lock(self->context);
    clReleaseMemObject(self->ptr);
    p1_cl_context_unlock(self->context);

    g_object_unref(self->context);
}

static gboolean p1_cl_memory_meta_transform(
    GstBuffer *transbuf, GstMeta *meta, GstBuffer *buffer, GQuark type, gpointer data)
{
    P1CLMemoryMeta *self = (P1CLMemoryMeta *)meta;

    P1CLMemoryMeta *copy = gst_buffer_add_cl_memory_meta(transbuf, self->context);
    GST_FIXME("copy not implemented for cl_mem");

    return copy != NULL;
}

const GstMetaInfo *p1_cl_memory_meta_get_info()
{
    static const GstMetaInfo *info = NULL;
    if (g_once_init_enter(&info)) {
        const GstMetaInfo *_info = gst_meta_register(
            P1_FRAME_BUFFER_META_API_TYPE,
            "P1CLMemoryMeta",
            sizeof(P1CLMemoryMeta),
            NULL,
            p1_cl_memory_meta_free,
            p1_cl_memory_meta_transform);
        g_once_init_leave(&info, _info);
    }
    return info;
}

GstBuffer *gst_buffer_new_cl_memory(P1CLContext *context, cl_mem ptr)
{
    GstBuffer *buf = gst_buffer_new();

    P1CLMemoryMeta *meta = gst_buffer_add_cl_memory_meta(buf, context);
    g_return_val_if_fail(meta != NULL, NULL);

    p1_cl_context_lock(context);
    meta->context = g_object_ref(context);
    meta->ptr = clRetainMemObject(ptr);
    p1_cl_context_unlock(context);

    return buf;
}
