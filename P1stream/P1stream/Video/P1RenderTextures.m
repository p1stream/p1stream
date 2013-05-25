#import "P1RenderTextures.h"
#import "P1TextureMeta.h"
#import "P1FrameBufferMeta.h"


G_DEFINE_TYPE(P1RenderTextures, p1_render_textures, GST_TYPE_ELEMENT)
static GstElementClass *parent_class = NULL;

static void p1_render_textures_dispose(
    GObject *self);
static void p1_render_textures_set_property(
    GObject *gobject, guint property_id, const GValue *value, GParamSpec *pspec);
static void p1_render_textures_get_property(
    GObject *gobject, guint property_id, GValue *value, GParamSpec *pspec);
static void p1_render_textures_set_context(
    P1RenderTextures *self, P1GLContext *context);
static void p1_render_textures_set_pool(
    P1RenderTextures *self, GstBufferPool *pool);
static GstStateChangeReturn p1_render_textures_change_state(
    GstElement *element, GstStateChange transition);
static GstPad *p1_render_textures_request_new_pad(
    GstElement *element, GstPadTemplate *templ, const gchar* name, const GstCaps *caps);
static void p1_render_textures_release_pad(
    GstElement *element, GstPad *pad);
static gboolean p1_render_textures_src_query(
    GstPad *pad, GstObject *parent, GstQuery *query);
static gboolean p1_render_textures_src_event(
    GstPad *pad, GstObject *parent, GstEvent *event);
static gboolean p1_render_textures_sink_query(
    GstCollectPads *collect, GstCollectData *data, GstQuery *query, gpointer user_data);
static gboolean p1_render_textures_sink_event(
    GstCollectPads *collect, GstCollectData *data, GstEvent *event, gpointer user_data);
static GstFlowReturn p1_render_textures_collected(
    GstCollectPads *collect, gpointer user_data);

enum
{
    PROP_0,
    PROP_WIDTH,
    PROP_HEIGHT
};

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE(
    "sink_%u", GST_PAD_SINK, GST_PAD_REQUEST, GST_STATIC_CAPS(
        "video/x-gl-texture, "
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

const GLsizei vbo_stride = 4 * sizeof(GLfloat);
const GLsizei vbo_size = 4 * vbo_stride;
const void *vbo_tex_coord_offset = (void *)(2 * sizeof(GLfloat));


static void p1_render_textures_class_init(P1RenderTexturesClass *klass)
{
    parent_class = g_type_class_ref(GST_TYPE_ELEMENT);

    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
    element_class->change_state    = p1_render_textures_change_state;
    element_class->request_new_pad = p1_render_textures_request_new_pad;
    element_class->release_pad     = p1_render_textures_release_pad;
    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&sink_template));
    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&src_template));
    gst_element_class_set_static_metadata(element_class, "P1stream render textures",
                                          "Filter/Video",
                                          "Renders a bunch of textures",
                                          "Stéphan Kochen <stephan@kochen.nl>");

    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->dispose = p1_render_textures_dispose;
    gobject_class->set_property = p1_render_textures_set_property;
    gobject_class->get_property = p1_render_textures_get_property;
    g_object_class_install_property(gobject_class, PROP_WIDTH,
        g_param_spec_int("width", "Output width", "Output video width",
                         16, 8192, 1280, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class, PROP_WIDTH,
        g_param_spec_int("height", "Output height", "Output video height",
                         16, 8192,  720, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void p1_render_textures_init(P1RenderTextures *self)
{
    GstPad *src = gst_pad_new_from_static_template(&src_template, "src");
    gst_pad_set_query_function(src, p1_render_textures_src_query);
    gst_pad_set_event_function(src, p1_render_textures_src_event);
    gst_element_add_pad(GST_ELEMENT(self), src);
    self->src = src;

    self->collect = gst_collect_pads_new();
    gst_collect_pads_set_query_function(self->collect, p1_render_textures_sink_query, self);
    gst_collect_pads_set_event_function(self->collect, p1_render_textures_sink_event, self);
    gst_collect_pads_set_function(self->collect, p1_render_textures_collected, self);

    self->width  = 1280;
    self->height =  720;
}

static void p1_render_textures_dispose(GObject *gobject)
{
    P1RenderTextures *self = P1_RENDER_TEXTURES(gobject);

    if (self->collect) {
        gst_object_unref(self->collect);
        self->collect = NULL;
    }

    p1_render_textures_set_pool(self, NULL);
    p1_render_textures_set_context(self, NULL);

    G_OBJECT_CLASS(parent_class)->dispose(gobject);
}

static void p1_render_textures_set_property(
    GObject *gobject, guint property_id, const GValue *value, GParamSpec *pspec)
{
    P1RenderTextures *self = P1_RENDER_TEXTURES(gobject);

    GST_OBJECT_LOCK(self);
    switch (property_id) {
        case PROP_WIDTH:
            self->width = g_value_get_int(value);
            self->send_caps = TRUE;
            break;
        case PROP_HEIGHT:
            self->height = g_value_get_int(value);
            self->send_caps = TRUE;
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(gobject, property_id, pspec);
            break;
    }
    GST_OBJECT_UNLOCK(self);
}

static void p1_render_textures_get_property(
    GObject *gobject, guint property_id, GValue *value, GParamSpec *pspec)
{
    P1RenderTextures *self = P1_RENDER_TEXTURES(gobject);

    switch (property_id) {
        case PROP_WIDTH:
            g_value_set_int(value, self->width);
            break;
        case PROP_HEIGHT:
            g_value_set_int(value, self->height);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(gobject, property_id, pspec);
            break;
    }
}

static void p1_render_textures_set_context(
    P1RenderTextures *self, P1GLContext *context)
{
    if (context == self->context)
        return;

    if (self->context != NULL) {
        p1_gl_context_lock(self->context);

        glDeleteBuffers(1, &self->vbo_name);
        glDeleteVertexArrays(1, &self->vao_name);
        glDeleteProgram(self->program_name);

        g_assert(glGetError() == GL_NO_ERROR);
        p1_gl_context_unlock(self->context);

        g_object_unref(self->context);
        self->context = NULL;
    }

    if (context != NULL) {
        GST_DEBUG_OBJECT(self, "setting context to %p", context);
        p1_gl_context_lock(context);

        // VBO and VAO for all drawing area vertex coordinates.
        glGenVertexArrays(1, &self->vao_name);
        glGenBuffers(1, &self->vbo_name);

        // Dirt simple shader.
        self->program_name = glCreateProgram();
        glBindAttribLocation(self->program_name, 0, "a_Position");
        glBindAttribLocation(self->program_name, 1, "a_TexCoords");
        glBindFragDataLocation(self->program_name, 0, "o_FragColor");
        p1_build_shader_program(self->program_name, @"simple");
        self->texture_uniform = glGetUniformLocation(self->program_name, "u_Texture");

        g_assert(glGetError() == GL_NO_ERROR);
        p1_gl_context_unlock(context);

        self->context = g_object_ref(context);
    }
}

static void p1_render_textures_set_pool(
    P1RenderTextures *self, GstBufferPool *pool)
{
    if (self->pool) {
        gst_buffer_pool_set_active(self->pool, FALSE);
        gst_object_unref(self->pool);
        self->pool = NULL;
    }

    if (pool) {
        gst_buffer_pool_set_active(pool, TRUE);
        self->pool = gst_object_ref(pool);
    }
}

static GstStateChangeReturn p1_render_textures_change_state(
    GstElement *element, GstStateChange transition)
{
    P1RenderTextures *self = P1_RENDER_TEXTURES(element);

    switch (transition) {
        case GST_STATE_CHANGE_NULL_TO_READY:
            break;
        case GST_STATE_CHANGE_READY_TO_PAUSED:
            gst_collect_pads_start(self->collect);
            self->send_stream_start = TRUE;
            self->send_caps = TRUE;
            break;
        case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
            break;
        case GST_STATE_CHANGE_PAUSED_TO_READY:
            gst_collect_pads_stop(self->collect);
            p1_render_textures_set_context(self, NULL);
            break;
        default:
            break;
    }

    return parent_class->change_state(element, transition);
}

static GstPad *p1_render_textures_request_new_pad(
    GstElement *element, GstPadTemplate *templ, const gchar* name, const GstCaps *caps)
{
    P1RenderTextures *self = P1_RENDER_TEXTURES(element);

    GstPad *sink = gst_pad_new_from_template(templ, name);
    GST_PAD_SET_PROXY_ALLOCATION(sink);

    if (gst_collect_pads_add_pad(self->collect, sink, sizeof(GstCollectData), NULL, TRUE) == NULL) {
        gst_object_unref(sink);
        return NULL;
    }

    if (gst_element_add_pad(element, sink) == FALSE) {
        gst_collect_pads_remove_pad(self->collect, sink);
        gst_object_unref(sink);
        return NULL;
    }

    return sink;
}

static void p1_render_textures_release_pad(GstElement *element, GstPad *pad)
{
    P1RenderTextures *self = P1_RENDER_TEXTURES(element);

    if (self->collect) {
        gst_collect_pads_remove_pad(self->collect, pad);
    }

    gst_element_remove_pad(element, pad);
}

static gboolean p1_render_textures_src_query(
    GstPad *pad, GstObject *parent, GstQuery *query)
{
    switch (GST_QUERY_TYPE(query)) {
        case GST_QUERY_CAPS: {
            GstElementClass *klass = GST_ELEMENT_GET_CLASS(parent);
            GstPadTemplate *template = gst_element_class_get_pad_template(klass, "src");
            GstCaps *caps = gst_pad_template_get_caps(template);

            GstCaps *filter;
            gst_query_parse_caps(query, &filter);
            if (filter) {
                GstCaps *intersection = gst_caps_intersect_full(filter, caps, GST_CAPS_INTERSECT_FIRST);
                gst_caps_unref(caps);
                caps = intersection;
            }

            gst_query_set_caps_result(query, caps);
            gst_caps_unref(caps);
            return TRUE;
        }
        default:
            return gst_pad_query_default(pad, parent, query);
    }
}

static gboolean p1_render_textures_src_event(
    GstPad *pad, GstObject *parent, GstEvent *event)
{
    switch (GST_EVENT_TYPE(event)) {
        default:
            return gst_pad_event_default(pad, parent, event);
    }
}

static gboolean p1_render_textures_sink_query(
    GstCollectPads *collect, GstCollectData *data, GstQuery *query, gpointer user_data)
{
    switch (GST_QUERY_TYPE(query)) {
        default:
            return gst_collect_pads_query_default(collect, data, query, FALSE);
    }
}

static gboolean p1_render_textures_sink_event(
    GstCollectPads *collect, GstCollectData *data, GstEvent *event, gpointer user_data)
{
    P1RenderTextures *self = (P1RenderTextures *)user_data;
    gboolean res = FALSE;

    switch (GST_EVENT_TYPE(event)) {
        case GST_EVENT_CAPS: {
            GstCaps *caps;
            gst_event_parse_caps(event, &caps);

            GstStructure *structure = gst_caps_get_structure(caps, 0);
            if (structure == NULL) {
                GST_ERROR_OBJECT(self, "missing context field in caps");
                break;
            }

            const GValue *context_value = gst_structure_get_value(structure, "context");
            if (context_value == NULL) {
                GST_ERROR_OBJECT(self, "no context specified in caps");
                break;
            }

            P1GLContext *context = g_value_get_object(context_value);
            if (self->context && context != self->context) {
                GST_ERROR_OBJECT(self, "upstream tried to change context mid-stream");
                break;
            }

            p1_render_textures_set_context(self, context);
            res = TRUE;
            break;
        }
        default:
            gst_event_ref(event);
            res = gst_collect_pads_event_default(collect, data, event, FALSE);
            break;
    }

    gst_event_unref(event);
    return res;
}

// FIXME: split this up
static GstFlowReturn p1_render_textures_collected(
    GstCollectPads *collect, gpointer user_data)
{
    P1RenderTextures *self = P1_RENDER_TEXTURES(user_data);
    GstFlowReturn ret;

    if (self->send_stream_start) {
        self->send_stream_start = FALSE;

        // FIXME: create id based on input ids
        gchar stream_id[32];
        g_snprintf(stream_id, sizeof(stream_id), "render-textures-%08x", g_random_int());
        gst_pad_push_event(self->src, gst_event_new_stream_start(stream_id));
    }

    GST_OBJECT_LOCK(self);
    const gint width  = self->width;
    const gint height = self->height;
    P1GLContext *context = self->context;
    gboolean send_caps = self->send_caps;
    self->send_caps = FALSE;
    GST_OBJECT_UNLOCK(self);

    if (send_caps) {
        // Build and send the caps event
        GstCaps *caps = gst_caps_new_simple("video/x-gl-texture",
            "width",  G_TYPE_INT, width,
            "height", G_TYPE_INT, height, NULL);

        GstStructure *structure = gst_caps_get_structure(caps, 0);
        GValue context_value = G_VALUE_INIT;
        g_value_init(&context_value, G_TYPE_OBJECT);
        g_value_set_object(&context_value, context);
        gst_structure_take_value(structure, "context", &context_value);

        gst_pad_push_event(self->src, gst_event_new_caps(caps));
        gst_caps_unref(caps);

        // Send an empty segment event
        GstSegment segment;
        gst_segment_init(&segment, GST_FORMAT_DEFAULT);
        gst_pad_push_event(self->src, gst_event_new_segment(&segment));

        // Query for a texture pool
        GstQuery *query = gst_query_new_allocation(caps, TRUE);
        gst_pad_peer_query(self->src, query);
        p1_decide_texture_allocation(query);

        GstBufferPool *pool = NULL;
        gst_query_parse_nth_allocation_pool(query, 0, &pool, NULL, NULL, NULL);
        p1_render_textures_set_pool(self, pool);
    }

    // Setup output
    if (!self->pool)
        return GST_FLOW_ERROR;
    p1_gl_context_lock(context);

    GstBuffer *outbuf;
    ret = gst_buffer_pool_acquire_buffer(self->pool, &outbuf, NULL);
    if (ret != GST_FLOW_OK) {
        p1_gl_context_unlock(context);
        return ret;
    }

    P1TextureMeta *texture = gst_buffer_get_texture_meta(outbuf);
    glBindTexture(GL_TEXTURE_RECTANGLE, texture->name);
    glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGBA, width, height,
                 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);

    P1FrameBufferMeta *fbo = gst_buffer_get_frame_buffer_meta(outbuf);
    if (fbo != NULL) {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo->name);
    }
    else {
        fbo = gst_buffer_add_frame_buffer_meta(outbuf, self->context);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo->name);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_RECTANGLE, texture->name, 0);
    }

    // Render
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glViewport(0, 0, width, height);
    glUseProgram(self->program_name);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(self->texture_uniform, 0);
    glBindVertexArray(self->vao_name);
    glBindBuffer(GL_ARRAY_BUFFER, self->vbo_name);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, vbo_stride, 0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, vbo_stride, vbo_tex_coord_offset);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    GLfloat vbo_data[4 * 4] = {
        -1, -1, 0, 0,
        -1, +1, 0, 1,
        +1, -1, 1, 0,
        +1, +1, 1, 1
    };

    // FIXME: sort
    for (GSList *collected = collect->data; collected != NULL; collected = g_slist_next(collected)) {
        GstCollectData *collect_data;
        GstBuffer *inbuf;

        collect_data = (GstCollectData *)collected->data;
        inbuf = gst_collect_pads_pop(collect, collect_data);
        if (inbuf == NULL)
            continue;

        texture = gst_buffer_get_texture_meta(inbuf);
        g_assert(p1_gl_context_is_shared(texture->context, context));
        glBindTexture(GL_TEXTURE_RECTANGLE, texture->name);

        // FIXME: set coordinates in vbo_data
        glBufferData(GL_ARRAY_BUFFER, vbo_size, vbo_data, GL_DYNAMIC_DRAW);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        gst_buffer_unref(inbuf);
    }

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    p1_gl_context_unlock(context);

    return gst_pad_push(self->src, outbuf);
}
