#import "P1TextureUpload.h"
#import "P1TexturePool.h"
#import "P1TextureMeta.h"
#import "P1IOSurfaceBuffer.h"
#import "P1Utils.h"


G_DEFINE_TYPE(P1TextureUpload, p1_texture_upload, GST_TYPE_BASE_TRANSFORM)
static GstBaseTransformClass *parent_class = NULL;

static void p1_texture_upload_dispose(
    GObject *gobject);
static gboolean p1_texture_upload_stop(
    GstBaseTransform *trans);
static void p1_texture_upload_set_context(
    P1TextureUpload *self, P1GLContext *context);
static GstCaps *p1_texture_upload_transform_caps(
    GstBaseTransform *trans, GstPadDirection direction, GstCaps *caps, GstCaps *filter);
static gboolean p1_texture_upload_set_caps(
    GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps);
static gboolean p1_texture_upload_decide_allocation(
    GstBaseTransform *trans, GstQuery *query);
static GstFlowReturn p1_texture_upload_transform(
    GstBaseTransform *trans, GstBuffer *inbuf, GstBuffer *outbuf);

// FIXME: different formats
static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE(
    "sink", GST_PAD_SINK, GST_PAD_ALWAYS, GST_STATIC_CAPS(
        "video/x-raw, "
            "format = (string) { BGRA }, "
            "width = (int) [ 1, max ], "
            "height = (int) [ 1, max ], "
            "framerate = (fraction) [ 0, max ]"
    )
);

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE(
    "src", GST_PAD_SRC, GST_PAD_ALWAYS, GST_STATIC_CAPS(
        "video/x-gl-texture, "
            "width = (int) [ 1, max ], "
            "height = (int) [ 1, max ], "
            "framerate = (fraction) [ 0, max ]"
    )
);


static void p1_texture_upload_class_init(P1TextureUploadClass *klass)
{
    parent_class = g_type_class_ref(GST_TYPE_BASE_TRANSFORM);

    GstBaseTransformClass *basetransform_class = GST_BASE_TRANSFORM_CLASS(klass);
    basetransform_class->stop              = p1_texture_upload_stop;
    basetransform_class->transform_caps    = p1_texture_upload_transform_caps;
    basetransform_class->set_caps          = p1_texture_upload_set_caps;
    basetransform_class->decide_allocation = p1_texture_upload_decide_allocation;
    // FIXME: propose_allocation with iosurface allocation
    basetransform_class->transform         = p1_texture_upload_transform;

    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&sink_template));
    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&src_template));
    gst_element_class_set_static_metadata(element_class, "P1stream texture upload",
                                           "Filter/Video",
                                           "Uploads a frame to an OpenGL texture",
                                           "Stéphan Kochen <stephan@kochen.nl>");

    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->dispose = p1_texture_upload_dispose;
}

static void p1_texture_upload_init(P1TextureUpload *self)
{
    self->context = NULL;
    self->width   = -1;
    self->height  = -1;

    GstBaseTransform *basetransform = GST_BASE_TRANSFORM(self);
    gst_base_transform_set_qos_enabled(basetransform, TRUE);
}

static void p1_texture_upload_dispose(GObject *gobject)
{
    P1TextureUpload *self = P1_TEXTURE_UPLOAD(gobject);

    p1_texture_upload_set_context(self, NULL);

    G_OBJECT_CLASS(parent_class)->dispose(gobject);
}

static gboolean p1_texture_upload_stop(GstBaseTransform *trans)
{
    P1TextureUpload *self = P1_TEXTURE_UPLOAD(trans);

    p1_texture_upload_set_context(self, NULL);

    return TRUE;
}

static void p1_texture_upload_set_context(P1TextureUpload *self, P1GLContext *context)
{
    if (context == self->context)
        return;

    if (self->context != NULL) {
        g_object_unref(self->context);
        self->context = NULL;
    }

    if (self->upload_context != NULL) {
        g_object_unref(self->upload_context);
        self->upload_context = NULL;
    }

    if (context != NULL) {
        GST_DEBUG_OBJECT(self, "setting context to %p", context);
        self->context = context;
        self->upload_context = p1_gl_context_new_shared(context);
    }
}

static GstCaps *p1_texture_upload_transform_caps(
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
            gst_structure_set_name(cap, "video/x-gl-texture");
            gst_structure_remove_field(cap, "format");
        }
        else {
            gst_structure_set_name(cap, "video/x-raw");
            gst_structure_set_value(cap, "format", &bgra_value);
            gst_structure_remove_field(cap, "context");
        }
    }

    g_value_unset(&bgra_value);
    return out;
}

static gboolean p1_texture_upload_set_caps(
    GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps)
{
    P1TextureUpload *self = P1_TEXTURE_UPLOAD(trans);
    GstStructure *structure = gst_caps_get_structure(outcaps, 0);

    // Determine the context to use
    GstQuery *query = gst_query_new_gl_context();
    gst_pad_peer_query(GST_BASE_TRANSFORM(self)->srcpad, query);
    P1GLContext *context = gst_query_get_gl_context(query);
    if (context != NULL) {
        if (self->context == NULL) {
            p1_texture_upload_set_context(self, g_object_ref(context));
        }
        else if (context != self->context) {
            GST_ERROR_OBJECT(self, "downstream tried to change context mid-stream");
            return FALSE;
        }
    }
    else if (self->context == NULL) {
        p1_texture_upload_set_context(self, p1_gl_context_new());
    }

    // Add the context to the downstream caps
    GValue context_value = G_VALUE_INIT;
    g_value_init(&context_value, G_TYPE_OBJECT);
    g_value_set_object(&context_value, self->context);
    gst_structure_take_value(structure, "context", &context_value);

    // Take width and height from the caps
    if (!gst_structure_get_int(structure, "width",  &self->width))
        return FALSE;
    if (!gst_structure_get_int(structure, "height", &self->height))
        return FALSE;

    return TRUE;
}

static gboolean p1_texture_upload_decide_allocation(
    GstBaseTransform *trans, GstQuery *query)
{
    return p1_decide_texture_allocation(query);
}

static GstFlowReturn p1_texture_upload_transform(
    GstBaseTransform *trans, GstBuffer *inbuf, GstBuffer *outbuf)
{
    gboolean success;
    P1TextureUpload *self = P1_TEXTURE_UPLOAD(trans);
    P1TextureMeta *meta = gst_buffer_get_texture_meta(outbuf);

    p1_gl_context_lock(self->upload_context);
    glBindTexture(GL_TEXTURE_RECTANGLE, meta->name);

    // Try for an IOSurface buffer.
    // FIXME: we can just add the meta to the existing buffer. But we need to
    // trigger in-place mode in GstBaseTransform.
    IOSurfaceRef surface = gst_buffer_get_iosurface(inbuf);
    if (surface != NULL) {
        // Check if the texture is already linked to this IOSurface.
        if (meta->dependency != inbuf) {
            CGLContextObj cglContext = p1_gl_context_get_raw(self->upload_context);
            CGLError err = CGLTexImageIOSurface2D(
                cglContext, GL_TEXTURE_RECTANGLE,
                GL_RGBA, self->width, self->height,
                GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, surface, 0);
            success = (err == kCGLNoError);
            if (success) {
                if (meta->dependency)
                    gst_buffer_unref(meta->dependency);
                meta->dependency = gst_buffer_ref(inbuf);
            }
        }
    }
    // Normal buffer to texture upload.
    else {
        GstMapInfo mapinfo;
        success = gst_buffer_map(inbuf, &mapinfo, GST_MAP_READ);
        if (success) {
            glTexImage2D(
                GL_TEXTURE_RECTANGLE, 0,
                GL_RGBA, self->width, self->height, 0,
                GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, mapinfo.data);
            gst_buffer_unmap(inbuf, &mapinfo);
        }
    }

    p1_gl_context_unlock(self->upload_context);
    return success ? GST_FLOW_OK : GST_FLOW_ERROR;
}
