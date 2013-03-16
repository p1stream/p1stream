#import "P1TextureUpload.h"
#import "P1TexturePool.h"
#import "P1TextureMeta.h"
#import "P1IOSurfaceBuffer.h"


G_DEFINE_TYPE(P1GTextureUpload, p1g_texture_upload, GST_TYPE_BASE_TRANSFORM)
static GstBaseTransformClass *parent_class = NULL;

static void p1g_texture_upload_src_dispose(GObject *gobject);
static GstCaps *p1g_texture_upload_transform_caps(
    GstBaseTransform *trans, GstPadDirection direction, GstCaps *caps, GstCaps *filter);
static gboolean p1g_texture_upload_set_caps(
    GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps);
static gboolean p1g_texture_upload_decide_allocation(
    GstBaseTransform *trans, GstQuery *query);
static GstFlowReturn p1g_texture_upload_transform(
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

// FIXME: different internal formats
static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE(
    "src", GST_PAD_SRC, GST_PAD_ALWAYS, GST_STATIC_CAPS(
        "video/x-gl-texture, "
            "width = (int) [ 1, max ], "
            "height = (int) [ 1, max ]"
    )
);


static void p1g_texture_upload_class_init(P1GTextureUploadClass *klass)
{
    parent_class = g_type_class_ref(GST_TYPE_BASE_TRANSFORM);

    GstBaseTransformClass *basetransform_class = GST_BASE_TRANSFORM_CLASS(klass);
    basetransform_class->transform_caps    = p1g_texture_upload_transform_caps;
    basetransform_class->set_caps          = p1g_texture_upload_set_caps;
    basetransform_class->decide_allocation = p1g_texture_upload_decide_allocation;
    // FIXME: propose_allocation with iosurface allocation
    basetransform_class->transform         = p1g_texture_upload_transform;

    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&sink_template));
    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&src_template));
    gst_element_class_set_static_metadata(element_class, "P1stream texture upload",
                                           "Filter/Video",
                                           "Uploads a frame as a texture to an OpenGL context",
                                           "Stéphan Kochen <stephan@kochen.nl>");

    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->dispose = p1g_texture_upload_src_dispose;
}

static void p1g_texture_upload_init(P1GTextureUpload *self)
{
    GstBaseTransform *basetransform = GST_BASE_TRANSFORM(self);
    gst_base_transform_set_qos_enabled(basetransform, TRUE);

    self->context = NULL;
}

static void p1g_texture_upload_src_dispose(GObject *gobject)
{
    P1GTextureUpload *self = P1G_TEXTURE_UPLOAD(gobject);

    if (self->context)
        g_object_unref(self->context);

    G_OBJECT_CLASS(parent_class)->dispose(gobject);
}

static GstCaps *p1g_texture_upload_transform_caps(
    GstBaseTransform *trans, GstPadDirection direction, GstCaps *caps, GstCaps *filter)
{
    GstCaps *out = gst_caps_copy(caps);
    guint size = gst_caps_get_size(out);
    for (guint i = 0; i < size; i++) {
        GstStructure *cap = gst_caps_get_structure(out, i);
        if (direction == GST_PAD_SINK) {
            gst_structure_set_name(cap, "video/x-gl-texture");
            gst_structure_remove_field(cap, "format");
        }
        else {
            GValue bgra_value = G_VALUE_INIT;
            g_value_init(&bgra_value, G_TYPE_STRING);
            g_value_set_static_string(&bgra_value, "BGRA");
            gst_structure_set_name(cap, "video/x-raw");
            gst_structure_take_value(cap, "format", &bgra_value);
        }
    }
    return out;
}

static gboolean p1g_texture_upload_set_caps(
    GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps)
{
    P1GTextureUpload *self = P1G_TEXTURE_UPLOAD(trans);

    GstStructure *structure = gst_caps_get_structure(incaps, 0);
    if (!gst_structure_get_int(structure, "width",  &self->width))
        return FALSE;
    if (!gst_structure_get_int(structure, "height", &self->height))
        return FALSE;

    return TRUE;
}

static gboolean p1g_texture_upload_decide_allocation(
    GstBaseTransform *trans, GstQuery *query)
{
    P1GTextureUpload *self = P1G_TEXTURE_UPLOAD(trans);

    P1GTexturePool *pool = NULL;
    guint size = 1, min = 1, max = 1;

    // Strip of all metas.
    guint n_metas = gst_query_get_n_allocation_metas(query);
    while (n_metas--) {
        gst_query_remove_nth_allocation_meta(query, n_metas);
    }

    // Strip of all allocators.
    guint n_params = gst_query_get_n_allocation_params(query);
    while (n_params--) {
        gst_query_set_nth_allocation_param(query, n_params, NULL, NULL);
    }

    // Keep only texture pools, and select the first in the list.
    GstBufferPool *i_pool;
    guint i_size, i_min, i_max;
    guint n_pools = gst_query_get_n_allocation_pools(query);
    while (n_pools--) {
        gst_query_parse_nth_allocation_pool(query, n_pools, &i_pool, &i_size, &i_min, &i_max);
        if (P1G_IS_TEXTURE_POOL(i_pool)) {
            pool = P1G_TEXTURE_POOL_CAST(i_pool);
            size = i_size;
            min = i_min;
            max = i_max;
        }
        else {
            gst_object_unref(i_pool);
            gst_query_set_nth_allocation_pool(query, n_pools, NULL, 0, 0, 0);
        }
    }

    // No pool, create our own from a new off-screen context.
    if (pool == NULL) {
        if (self->context != NULL) {
            if (p1g_opengl_context_get_parent(self->context) != NULL) {
                g_object_unref(self->context);
                self->context = NULL;
            }
        }

        if (self->context == NULL) {
            self->context = p1g_opengl_context_new(NULL);
            g_return_val_if_fail(self->context != NULL, FALSE);
        }

        pool = p1g_texture_pool_new(self->context);
        g_return_val_if_fail(pool != NULL, FALSE);
    }
    // Downstream gave us a pool, upload to it from a shared context.
    else {
        P1GOpenGLContext *parent_context = p1g_texture_pool_get_context(pool);

        if (self->context != NULL) {
            if (!p1g_opengl_context_is_shared_with(self->context, parent_context)) {
                g_object_unref(self->context);
                self->context = NULL;
            }
        }

        if (self->context == NULL) {
            self->context = p1g_opengl_context_new(parent_context);
            g_return_val_if_fail(self->context != NULL, FALSE);

            gst_object_unref(parent_context);
        }
    }

    GstCaps *outcaps;
    gst_query_parse_allocation(query, &outcaps, NULL);

    GstAllocationParams params;
    GstStructure *config = gst_buffer_pool_get_config(GST_BUFFER_POOL(pool));
    gst_buffer_pool_config_set_params(config, outcaps, size, min, max);
    gst_buffer_pool_config_set_allocator(config, NULL, &params);
    gst_buffer_pool_set_config(GST_BUFFER_POOL(pool), config);

    // Fix the pool selection.
    gst_query_set_nth_allocation_pool(query, 0, GST_BUFFER_POOL_CAST(pool), size, min, max);
    gst_object_unref(pool);

    return TRUE;
}

static GstFlowReturn p1g_texture_upload_transform(
    GstBaseTransform *trans, GstBuffer *inbuf, GstBuffer *outbuf)
{
    P1GTextureUpload *self = P1G_TEXTURE_UPLOAD(trans);
    P1GTextureMeta *meta = gst_buffer_get_texture_meta(outbuf);

    p1g_opengl_context_activate(self->context);
    glBindTexture(GL_TEXTURE_RECTANGLE, meta->texture_name);

    // Try for an IOSurface buffer.
    IOSurfaceRef surface = gst_buffer_get_iosurface(inbuf);
    if (surface != NULL) {
        // Check if the texture is already linked to this IOSurface.
        if (meta->dependency == inbuf)
            return GST_FLOW_OK;

        CGLError err = CGLTexImageIOSurface2D(
            self->context->context, GL_TEXTURE_RECTANGLE,
            GL_RGBA, self->width, self->height,
            GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, surface, 0);
        g_return_val_if_fail(err == kCGLNoError, GST_FLOW_ERROR);

        meta->dependency = gst_buffer_ref(inbuf);
    }
    // Normal buffer to texture upload.
    else {
        GstMapInfo mapinfo;
        gboolean success = gst_buffer_map(inbuf, &mapinfo, GST_MAP_READ);
        g_return_val_if_fail(success, GST_FLOW_ERROR);

        glTexImage2D(
            GL_TEXTURE_RECTANGLE, 0,
            GL_RGBA, self->width, self->height, 0,
            GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, mapinfo.data);

        gst_buffer_unmap(inbuf, &mapinfo);
    }

    return GST_FLOW_OK;
}
