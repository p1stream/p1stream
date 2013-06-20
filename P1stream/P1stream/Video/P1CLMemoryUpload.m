#import "P1CLMemoryUpload.h"
//#import "P1CLMemoryPool.h" // FIXME
#import "P1CLMemoryMeta.h"
#import "P1IOSurfaceBuffer.h"


G_DEFINE_TYPE(P1CLMemoryUpload, p1_cl_memory_upload, GST_TYPE_BASE_TRANSFORM)
static GstBaseTransformClass *parent_class = NULL;

static void p1_cl_memory_upload_dispose(
    GObject *gobject);
static gboolean p1_cl_memory_upload_stop(
    GstBaseTransform *trans);
static void p1_cl_memory_upload_set_context(
    P1CLMemoryUpload *self, P1CLContext *context);
static GstCaps *p1_cl_memory_upload_transform_caps(
    GstBaseTransform *trans, GstPadDirection direction, GstCaps *caps, GstCaps *filter);
static gboolean p1_cl_memory_upload_set_caps(
    GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps);
static gboolean p1_cl_memory_upload_decide_allocation(
    GstBaseTransform *trans, GstQuery *query);
static GstFlowReturn p1_cl_memory_upload_transform(
    GstBaseTransform *trans, GstBuffer *inbuf, GstBuffer *outbuf);

// FIXME: different formats
static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE(
    "sink", GST_PAD_SINK, GST_PAD_ALWAYS, GST_STATIC_CAPS(
        "video/x-raw, "
            "format = (string) { BGRA }, "
            "width = (int) [ 1, max ], "
            "height = (int) [ 1, max ]"
    )
);

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE(
    "src", GST_PAD_SRC, GST_PAD_ALWAYS, GST_STATIC_CAPS(
        "video/x-gl-texture, "
            "width = (int) [ 1, max ], "
            "height = (int) [ 1, max ]"
    )
);


static void p1_cl_memory_upload_class_init(P1CLMemoryUploadClass *klass)
{
    parent_class = g_type_class_ref(GST_TYPE_BASE_TRANSFORM);

    GstBaseTransformClass *basetransform_class = GST_BASE_TRANSFORM_CLASS(klass);
    basetransform_class->stop              = p1_cl_memory_upload_stop;
    basetransform_class->transform_caps    = p1_cl_memory_upload_transform_caps;
    basetransform_class->set_caps          = p1_cl_memory_upload_set_caps;
    basetransform_class->decide_allocation = p1_cl_memory_upload_decide_allocation;
    // FIXME: propose_allocation with iosurface allocation
    basetransform_class->transform         = p1_cl_memory_upload_transform;

    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&sink_template));
    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&src_template));
    gst_element_class_set_static_metadata(element_class, "P1stream OpenCL upload",
                                           "Filter/Video",
                                           "Uploads a buffer to OpenCL memory",
                                           "Stéphan Kochen <stephan@kochen.nl>");

    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->dispose = p1_cl_memory_upload_dispose;
}

static void p1_cl_memory_upload_init(P1CLMemoryUpload *self)
{
    GstBaseTransform *basetransform = GST_BASE_TRANSFORM(self);
    gst_base_transform_set_qos_enabled(basetransform, TRUE);
}

static void p1_cl_memory_upload_dispose(GObject *gobject)
{
    P1CLMemoryUpload *self = P1_CL_MEMORY_UPLOAD(gobject);

    p1_cl_memory_upload_set_context(self, NULL);

    G_OBJECT_CLASS(parent_class)->dispose(gobject);
}

static gboolean p1_cl_memory_upload_stop(GstBaseTransform *trans)
{
    P1CLMemoryUpload *self = P1_CL_MEMORY_UPLOAD(trans);

    p1_cl_memory_upload_set_context(self, NULL);

    return TRUE;
}

static void p1_cl_memory_upload_set_context(P1CLMemoryUpload *self, P1CLContext *context)
{
    if (context == self->context)
        return;

    if (self->context != NULL) {
        g_object_unref(self->context);
        self->context = NULL;
    }

    if (context != NULL) {
        GST_DEBUG_OBJECT(self, "setting context to %p", context);
        self->context = context;
    }
}

static GstCaps *p1_cl_memory_upload_transform_caps(
    GstBaseTransform *trans, GstPadDirection direction, GstCaps *caps, GstCaps *filter)
{
    // FIXME
    return NULL;
}

static gboolean p1_cl_memory_upload_set_caps(
    GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps)
{
    gboolean res = TRUE;
    P1CLMemoryUpload *self = P1_CL_MEMORY_UPLOAD(trans);
    GstStructure *structure = gst_caps_get_structure(outcaps, 0);
    const gchar *caps_name = gst_structure_get_name(structure);

    // Determine the context to use
    if (strcmp(caps_name, "video/x-gl-texture") == 0) {
        // Upstream is GL, get the GL context
        const GValue *context_value = gst_structure_get_value(structure, "context");
        g_return_val_if_fail(context_value != NULL, FALSE);
        P1GLContext *gl_context = g_value_get_object(context_value);
        g_return_val_if_fail(gl_context != NULL, FALSE);

        // Ensure we have a context shared with the GL context
        if (self->context == NULL) {
            P1CLContext *context = p1_cl_context_new_shared_with_gl(gl_context);
            p1_cl_memory_upload_set_context(self, context);
        }
        else if (p1_cl_context_get_parent(self->context) != gl_context) {
            GST_ERROR_OBJECT(self, "upstream tried to change context mid-stream");
            res = FALSE;
        }
    }
    else {
        // Query downstream for an existing context
        GstQuery *query = gst_query_new_cl_context();
        gst_pad_peer_query(GST_BASE_TRANSFORM(self)->srcpad, query);
        P1CLContext *context = gst_query_get_cl_context(query);

        if (context != NULL) {
            if (self->context == NULL) {
                p1_cl_memory_upload_set_context(self, g_object_ref(context));
            }
            else if (context != self->context) {
                GST_ERROR_OBJECT(self, "upstream tried to change context mid-stream");
                res = FALSE;
            }
        }
        else if (self->context == NULL) {
            p1_cl_memory_upload_set_context(self, p1_cl_context_new());
        }

        gst_query_unref(query);
    }

    // Add the context to the downstream caps
    if (res != FALSE) {
        GValue context_value = G_VALUE_INIT;
        g_value_init(&context_value, G_TYPE_OBJECT);
        g_value_set_object(&context_value, self->context);
        gst_structure_take_value(structure, "context", &context_value);
    }

    return res;
}

static gboolean p1_cl_memory_upload_decide_allocation(
    GstBaseTransform *trans, GstQuery *query)
{
    gst_query_strip_allocation_metas(query);
    gst_query_strip_allocation_params(query);

    // Keep only cl_mem pools, and select the first in the list.
    GstBufferPool *pool = NULL;
    guint size, min, max;
    guint num = gst_query_get_n_allocation_pools(query);
    // FIXME: implement pool
#if 1
    size = min = max = 0;
#else
    for (guint i = 0; i < num; i++) {
        gst_query_parse_nth_allocation_pool(query, i, &pool, &size, &min, &max);
        if (P1_IS_CL_MEMORY_POOL(pool))
            break;

        gst_object_unref(pool);
        pool = NULL;
    }

    // No texture pool, create our own.
    if (pool == NULL) {
        pool = GST_BUFFER_POOL_CAST(p1_cl_memory_pool_new());
        g_return_val_if_fail(pool != NULL, FALSE);
        size = 1;
        min = max = 0;
    }

    // Extract caps, which we need to set on the pool config
    GstCaps *outcaps;
    gst_query_parse_allocation(query, &outcaps, NULL);

    // Build the pool config
    GstStructure *config = gst_buffer_pool_get_config(pool);
    gst_buffer_pool_config_set_params(config, outcaps, size, min, max);
    gst_buffer_pool_set_config(pool, config);
#endif

    // Fix the pool selection.
    if (num == 0)
        gst_query_add_allocation_pool(query, pool, size, min, max);
    else
        gst_query_set_nth_allocation_pool(query, 0, pool, size, min, max);
    gst_object_unref(pool);

    return TRUE;
}

static GstFlowReturn p1_cl_memory_upload_transform(
    GstBaseTransform *trans, GstBuffer *inbuf, GstBuffer *outbuf)
{
    // FIXME
    return GST_FLOW_NOT_SUPPORTED;
}
