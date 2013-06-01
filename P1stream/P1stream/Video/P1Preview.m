#import "P1Preview.h"
#import "P1TextureMeta.h"
#import "P1TexturePool.h"
#import "P1Utils.h"


G_DEFINE_TYPE(P1PreviewSink, p1_preview_sink, GST_TYPE_VIDEO_SINK)
static GstVideoSinkClass *parent_class = NULL;

static gboolean p1_preview_sink_query(
    GstBaseSink *basesink, GstQuery *query);
static gboolean p1_preview_sink_set_caps(
    GstBaseSink *basesink, GstCaps *caps);
static GstStateChangeReturn p1_preview_sink_change_state(
    GstElement *element, GstStateChange transition);
static GstFlowReturn p1_preview_sink_show_frame(
    GstVideoSink *videosink, GstBuffer *buf);

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE(
    "sink", GST_PAD_SINK, GST_PAD_ALWAYS, GST_STATIC_CAPS(
        "video/x-gl-texture, "
            "width = (int) [ 1, max ], "
            "height = (int) [ 1, max ]"
    )
);


static void p1_preview_sink_class_init(P1PreviewSinkClass *klass)
{
    parent_class = g_type_class_ref(GST_TYPE_VIDEO_SINK);

    GstVideoSinkClass *videosink_class = GST_VIDEO_SINK_CLASS(klass);
    videosink_class->show_frame = p1_preview_sink_show_frame;

    GstBaseSinkClass *basesink_class = GST_BASE_SINK_CLASS(klass);
    basesink_class->query    = p1_preview_sink_query;
    basesink_class->set_caps = p1_preview_sink_set_caps;

    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
    element_class->change_state = p1_preview_sink_change_state;
    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&sink_template));
    gst_element_class_set_static_metadata(element_class, "P1stream preview sink",
                                           "Sink/Video",
                                           "The P1stream video preview",
                                           "Stéphan Kochen <stephan@kochen.nl>");
}

static void p1_preview_sink_init(P1PreviewSink *self)
{
    self->viewRef = NULL;
}

static gboolean p1_preview_sink_query(GstBaseSink *basesink, GstQuery *query)
{
    gboolean res = FALSE;
    P1PreviewSink *self = P1_PREVIEW_SINK(basesink);

    @autoreleasepool {
        P1Preview *view = (__bridge P1Preview *)self->viewRef;

        switch ((int)GST_QUERY_TYPE(query)) {
            case GST_QUERY_GL_CONTEXT: {
                P1GLContext *actual = view.gobjContext;
                P1GLContext *current = gst_query_get_gl_context(query);
                if (current == actual)
                    res = TRUE;
                else if (current == NULL)
                    res = gst_query_set_gl_context(query, actual);
                else
                    GST_ERROR_OBJECT(self, "multiple contexts in response to query");
                break;
            }
            default:
                res = GST_BASE_SINK_CLASS(parent_class)->query(basesink, query);
                break;
        }
    }

    return res;
}

static gboolean p1_preview_sink_set_caps(GstBaseSink *basesink, GstCaps *caps)
{
    P1PreviewSink *self = P1_PREVIEW_SINK(basesink);

    @autoreleasepool {
        P1Preview *view = (__bridge P1Preview *)self->viewRef;
        GstStructure *structure = gst_caps_get_structure(caps, 0);

        const GValue *context_value = gst_structure_get_value(structure, "context");
        P1GLContext *current = g_value_get_object(context_value);
        P1GLContext *actual = view.gobjContext;
        if (current != actual) {
            GST_ERROR_OBJECT(self, "caps context does not match");
            return FALSE;
        }

        gint width, height;
        if (!gst_structure_get_int(structure, "width",  &width))
            return FALSE;
        if (!gst_structure_get_int(structure, "height", &height))
            return FALSE;
        view.aspect = (CGFloat)width / (CGFloat)height;

        dispatch_sync(dispatch_get_main_queue(), ^{
            [view updateVideoConstraint];
        });
    }

    return TRUE;
}

static GstStateChangeReturn p1_preview_sink_change_state(GstElement *element, GstStateChange transition)
{
    GstStateChangeReturn res;
    P1PreviewSink *self = P1_PREVIEW_SINK(element);

    @autoreleasepool {
        P1Preview *view = (__bridge P1Preview *)self->viewRef;

        switch (transition) {
            case GST_STATE_CHANGE_NULL_TO_READY:
                // Ref our parent view while we're active.
                g_assert(self->viewRef != NULL);
                CFRetain(self->viewRef);
                break;
            default:
                break;
        }

        res = GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);
        if (res == GST_STATE_CHANGE_FAILURE)
            return res;

        switch (transition) {
            case GST_STATE_CHANGE_PAUSED_TO_READY:
                view.buffer = NULL;
                [view performSelectorOnMainThread:@selector(clearVideoConstraint)
                                       withObject:nil
                                    waitUntilDone:TRUE];
                break;
            case GST_STATE_CHANGE_READY_TO_NULL:
                CFRelease(self->viewRef);
                break;
            default:
                break;
        }
    }

    return res;
}

static GstFlowReturn p1_preview_sink_show_frame(GstVideoSink *videosink, GstBuffer *buf)
{
    P1PreviewSink *self = P1_PREVIEW_SINK(videosink);

    @autoreleasepool {
        P1Preview *view = (__bridge P1Preview *)self->viewRef;
        view.buffer = buf;
    }

    return GST_FLOW_OK;
}


@implementation P1Preview

@synthesize element, aspect;

// VBO data for a square with texture coordinates flipped.
static const GLfloat vboData[4 * 4] = {
    -1, -1, 0, 0,
    -1, +1, 0, 1,
    +1, -1, 1, 0,
    +1, +1, 1, 1
};
const GLsizei vboSize = sizeof(vboData);
const GLsizei vboStride = 4 * sizeof(GLfloat);
const void *vboTexCoordsOffset = (void *)(2 * sizeof(GLfloat));

- (id)initWithFrame:(NSRect)frameRect
{
    // Set up context with 3.2 core profile.
    NSOpenGLPixelFormat *pixelFormat;
    NSOpenGLPixelFormatAttribute attrs[] = {
        NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core,
        0
    };
    pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];

    // Chain up.
    self = [super initWithFrame:frameRect pixelFormat:pixelFormat];
    if (self) {
        // Create the GStreamer element.
        element = g_object_new(P1_TYPE_PREVIEW_SINK, NULL);
        g_assert(element != NULL);
        element->viewRef = (__bridge CFTypeRef)self;

        // Start context init.
        NSOpenGLContext *context = self.openGLContext;
        [context makeCurrentContext];

        // Black background.
        glClearColor(0, 0, 0, 1);

        // VBO and VAO for all drawing area vertex coordinates.
        glGenVertexArrays(1, &vertexArrayName);
        glGenBuffers(1, &vertexBufferName);
        glBindBuffer(GL_ARRAY_BUFFER, vertexBufferName);
        glBufferData(GL_ARRAY_BUFFER, vboSize, vboData, GL_STATIC_DRAW);

        // Dirt simple shader.
        shaderProgram = glCreateProgram();
        glBindAttribLocation(shaderProgram, 0, "a_Position");
        glBindAttribLocation(shaderProgram, 1, "a_TexCoords");
        glBindFragDataLocation(shaderProgram, 0, "o_FragColor");
        p1_build_shader_program(shaderProgram, @"simple");
        textureUniform = glGetUniformLocation(shaderProgram, "u_Texture");

        g_assert(glGetError() == GL_NO_ERROR);
    }
    return self;
}

- (void)dealloc
{
    if (element) {
        element->viewRef = NULL;
        gst_object_unref(element);
    }

    if (gobjContext) {
        g_object_unref(gobjContext);
    }
}

- (CGLContextObj)CGLContextObj
{
    @synchronized(self) {
        return self.openGLContext.CGLContextObj;
    }
}

- (P1GLContext *)gobjContext
{
    @synchronized(self) {
        CGLContextObj actual = self.CGLContextObj;

        if (gobjContext != NULL) {
            CGLContextObj current = p1_gl_context_get_raw(gobjContext);
            if (current == actual)
                return gobjContext;
            g_object_unref(gobjContext);
        }

        gobjContext = p1_gl_context_new_existing(actual);
        return gobjContext;
    }
}

- (void)updateVideoConstraint
{
    [self clearVideoConstraint];
    videoConstraint = [NSLayoutConstraint constraintWithItem:self
                                                   attribute:NSLayoutAttributeWidth
                                                   relatedBy:NSLayoutRelationEqual
                                                      toItem:self
                                                   attribute:NSLayoutAttributeHeight
                                                  multiplier:aspect
                                                    constant:0];
    [self addConstraint:videoConstraint];
}

- (void)clearVideoConstraint
{
    if (videoConstraint) {
        [self removeConstraint:videoConstraint];
        videoConstraint = nil;
    }
}

- (void)setBuffer:(GstBuffer *)buffer
{
    @synchronized(self) {
        if (currentBuffer)
            gst_buffer_unref(currentBuffer);

        if (buffer)
            currentBuffer = gst_buffer_ref(buffer);
        else
            currentBuffer = NULL;

        [self lockFocus];
        [self.openGLContext makeCurrentContext];
        [self drawBuffer];
        [self unlockFocus];
    }
}

- (BOOL)isOpaque
{
    return TRUE;
}

- (void)drawRect:(NSRect)dirtyRect
{
    [self drawBuffer];
}

- (void)drawBuffer
{
    CGLError err;

    CGLContextObj context = self.openGLContext.CGLContextObj;
    err = CGLLockContext(context);
    assert(err == kCGLNoError);
    glClear(GL_COLOR_BUFFER_BIT);

    GstBuffer *buf = self->currentBuffer;
    if (buf != NULL) {
        P1TextureMeta *texture = gst_buffer_get_texture_meta(buf);
        g_assert(p1_gl_context_get_raw(texture->context) == context);

        const NSRect bounds = self.bounds;
        glViewport(bounds.origin.x, bounds.origin.y, bounds.size.width, bounds.size.height);

        glUseProgram(shaderProgram);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_RECTANGLE, texture->name);
        glUniform1i(textureUniform, 0);

        glBindVertexArray(vertexArrayName);
        glBindBuffer(GL_ARRAY_BUFFER, vertexBufferName);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, vboStride, 0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, vboStride, vboTexCoordsOffset);
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
    }

    glFlush();
    err = CGLUnlockContext(context);
    assert(err == kCGLNoError);
}

@end
