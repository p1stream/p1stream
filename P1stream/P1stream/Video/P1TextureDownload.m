#include <gst/video/video.h>
#include <gst/video/gstvideopool.h>
#import "P1TextureDownload.h"
#import "P1TexturePool.h"
#import "P1TextureMeta.h"
#import "P1IOSurfaceBuffer.h"
#import "P1Utils.h"


G_DEFINE_TYPE(P1TextureDownload, p1_texture_download, GST_TYPE_BASE_TRANSFORM)
static GstBaseTransformClass *parent_class = NULL;

static void p1_texture_download_dispose(
    GObject *gobject);
static gboolean p1_texture_download_stop(
    GstBaseTransform *trans);
static void p1_texture_download_set_context(
    P1TextureDownload *self, P1GLContext *context);
static GstCaps *p1_texture_download_transform_caps(
    GstBaseTransform *trans, GstPadDirection direction, GstCaps *caps, GstCaps *filter);
static gboolean p1_texture_download_set_caps(
    GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps);
static gboolean p1_texture_download_query(
    GstBaseTransform *trans, GstPadDirection direction, GstQuery *query);
static gboolean p1_texture_download_decide_allocation(
    GstBaseTransform *trans, GstQuery *query);
static GstFlowReturn p1_texture_download_prepare_output_buffer(
    GstBaseTransform *trans, GstBuffer *inbuf, GstBuffer **outbuf);
static GstFlowReturn p1_texture_download_transform(
    GstBaseTransform *trans, GstBuffer *inbuf, GstBuffer *outbuf);

// FIXME: different formats
static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE(
    "sink", GST_PAD_SINK, GST_PAD_ALWAYS, GST_STATIC_CAPS(
        "video/x-gl-texture, "
            "width = (int) [ 1, max ], "
            "height = (int) [ 1, max ], "
            "framerate = (fraction) [ 0, max ]"
    )
);

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE(
    "src", GST_PAD_SRC, GST_PAD_ALWAYS, GST_STATIC_CAPS(
        "video/x-raw, "
            "format = (string) { BGRA }, "
            "width = (int) [ 1, max ], "
            "height = (int) [ 1, max ], "
            "framerate = (fraction) [ 0, max ]"
    )
);


static void p1_texture_download_class_init(P1TextureDownloadClass *klass)
{
    parent_class = g_type_class_ref(GST_TYPE_BASE_TRANSFORM);

    GstBaseTransformClass *basetransform_class = GST_BASE_TRANSFORM_CLASS(klass);
    basetransform_class->transform_caps        = p1_texture_download_transform_caps;
    basetransform_class->set_caps              = p1_texture_download_set_caps;
    basetransform_class->query                 = p1_texture_download_query;
    basetransform_class->decide_allocation     = p1_texture_download_decide_allocation;
    basetransform_class->prepare_output_buffer = p1_texture_download_prepare_output_buffer;
    basetransform_class->transform             = p1_texture_download_transform;
    basetransform_class->stop                  = p1_texture_download_stop;

    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&sink_template));
    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&src_template));
    gst_element_class_set_static_metadata(element_class, "P1stream texture download",
                                           "Filter/Video",
                                           "Downloads a frame from an OpenGL texture",
                                           "Stéphan Kochen <stephan@kochen.nl>");

    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->dispose = p1_texture_download_dispose;
}

static void p1_texture_download_init(P1TextureDownload *self)
{
    GstBaseTransform *basetransform = GST_BASE_TRANSFORM(self);
    gst_base_transform_set_qos_enabled(basetransform, TRUE);
}

static void p1_texture_download_dispose(GObject *gobject)
{
    P1TextureDownload *self = P1_TEXTURE_DOWNLOAD(gobject);

    p1_texture_download_set_context(self, NULL);

    G_OBJECT_CLASS(parent_class)->dispose(gobject);
}

static gboolean p1_texture_download_stop(GstBaseTransform *trans)
{
    P1TextureDownload *self = P1_TEXTURE_DOWNLOAD(trans);

    p1_texture_download_set_context(self, NULL);

    return TRUE;
}

static void p1_texture_download_set_context(P1TextureDownload *self, P1GLContext *context)
{
    if (context == self->context)
        return;

    if (self->context != NULL) {
        g_object_unref(self->context);
        self->context = NULL;
    }

    if (self->download_context != NULL) {
        g_object_unref(self->download_context);
        self->download_context = NULL;
    }

    if (context != NULL) {
        GST_DEBUG_OBJECT(self, "setting context to %p", context);
        self->context = context;
        self->download_context = p1_gl_context_new_shared(context);
    }
}

static GstCaps *p1_texture_download_transform_caps(
    GstBaseTransform *trans, GstPadDirection direction, GstCaps *caps, GstCaps *filter)
{
    // Prepare BGRA string value
    GValue bgra_value = G_VALUE_INIT;
    g_value_init(&bgra_value, G_TYPE_STRING);
    g_value_set_static_string(&bgra_value, "BGRA");

    // Transform each structure
    GstCaps *out = gst_caps_copy(caps);
    guint size = gst_caps_get_size(out);
    for (guint i = 0; i < size; i++) {
        GstStructure *cap = gst_caps_get_structure(out, i);
        if (direction == GST_PAD_SINK) {
            gst_structure_set_name(cap, "video/x-raw");
            gst_structure_set_value(cap, "format", &bgra_value);
            gst_structure_remove_field(cap, "context");
        }
        else {
            gst_structure_set_name(cap, "video/x-gl-texture");
            gst_structure_remove_field(cap, "format");
        }
    }

    g_value_unset(&bgra_value);
    return out;
}

static gboolean p1_texture_download_set_caps(
    GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps)
{
    P1TextureDownload *self = P1_TEXTURE_DOWNLOAD(trans);
    GstStructure *structure = gst_caps_get_structure(incaps, 0);

    // Determine the context to use
    const GValue *context_value = gst_structure_get_value(structure, "context");
    g_return_val_if_fail(context_value != NULL, FALSE);
    P1GLContext *context = g_value_get_object(context_value);
    g_return_val_if_fail(context != NULL, FALSE);
    if (self->context == NULL) {
        p1_texture_download_set_context(self, g_object_ref(context));
    }
    else if (context != self->context) {
        GST_ERROR_OBJECT(self, "upstream tried to change context mid-stream");
        return FALSE;
    }

    return TRUE;
}

static gboolean p1_texture_download_query(
    GstBaseTransform *trans, GstPadDirection direction, GstQuery *query)
{
    gboolean res = FALSE;

    switch ((int)GST_QUERY_TYPE(query)) {
        case GST_QUERY_GL_CONTEXT:
            break; // Don't forward
        default:
            res = parent_class->query(trans, direction, query);
    }

    return res;
}

static gboolean p1_texture_download_decide_allocation(
    GstBaseTransform *trans, GstQuery *query)
{
    // Taken from GstVideoFilter.
    GstBufferPool *pool = NULL;
    GstStructure *config;
    guint min, max, size;
    gboolean update_pool;
    GstCaps *outcaps;

    gst_query_parse_allocation(query, &outcaps, NULL);

    if (gst_query_get_n_allocation_pools(query) > 0) {
        gst_query_parse_nth_allocation_pool(query, 0, &pool, &size, &min, &max);
        update_pool = TRUE;
    } else {
        GstVideoInfo vinfo;
        gst_video_info_init(&vinfo);
        gst_video_info_from_caps(&vinfo, outcaps);

        size = (guint)vinfo.size;
        min = max = 0;
        update_pool = FALSE;
    }

    if (!pool)
        pool = gst_video_buffer_pool_new();

    config = gst_buffer_pool_get_config(pool);
    gst_buffer_pool_config_add_option(config, GST_BUFFER_POOL_OPTION_VIDEO_META);
    gst_buffer_pool_config_set_params(config, outcaps, size, min, max);
    gst_buffer_pool_set_config(pool, config);

    if (update_pool)
        gst_query_set_nth_allocation_pool(query, 0, pool, size, min, max);
    else
        gst_query_add_allocation_pool(query, pool, size, min, max);

    gst_object_unref(pool);

    return parent_class->decide_allocation(trans, query);
}

static GstFlowReturn p1_texture_download_prepare_output_buffer(
    GstBaseTransform *trans, GstBuffer *inbuf, GstBuffer **outbuf)
{
    // Try for a texture dependency, which we can pass through.
    P1TextureMeta *texture = gst_buffer_get_texture_meta(inbuf);
    if (texture->dependency != NULL) {
        *outbuf = gst_buffer_ref(texture->dependency);
        return GST_FLOW_OK;
    }
    // Normal texture allocation.
    else {
        return parent_class->prepare_output_buffer(trans, inbuf, outbuf);
    }
}

static GstFlowReturn p1_texture_download_transform(
    GstBaseTransform *trans, GstBuffer *inbuf, GstBuffer *outbuf)
{
    P1TextureDownload *self = P1_TEXTURE_DOWNLOAD(trans);
    P1TextureMeta *texture = gst_buffer_get_texture_meta(inbuf);
    gboolean success = TRUE;

    if (outbuf != texture->dependency) {
        GstMapInfo mapinfo;
        success = gst_buffer_map(outbuf, &mapinfo, GST_MAP_READ);
        if (success) {
            p1_gl_context_lock(self->download_context);
            
            glBindTexture(GL_TEXTURE_RECTANGLE, texture->name);
            glGetTexImage(
                GL_TEXTURE_RECTANGLE, 0,
                GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, mapinfo.data);
            
            p1_gl_context_unlock(self->download_context);
            gst_buffer_unmap(inbuf, &mapinfo);
        }
    }

    return success ? GST_FLOW_OK : GST_FLOW_ERROR;
}
