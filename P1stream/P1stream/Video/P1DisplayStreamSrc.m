#import "P1DisplayStreamSrc.h"
#import "P1IOSurfaceBuffer.h"


G_DEFINE_TYPE(P1GDisplayStreamSrc, p1g_display_stream_src, GST_TYPE_PUSH_SRC)
static GstPushSrcClass *parent_class = NULL;

static void p1g_display_stream_src_dispose(GObject *gobject);
static GstCaps *p1g_display_stream_src_get_caps(GstBaseSrc *src, GstCaps *filter);
static gboolean p1g_display_stream_src_start(GstBaseSrc *basesrc);
static gboolean p1g_display_stream_src_stop(GstBaseSrc *basesrc);
static gboolean p1g_display_stream_src_unlock(GstBaseSrc *basesrc);
static gboolean p1g_display_stream_src_unlock_stop(GstBaseSrc *basesrc);
static void p1g_display_stream_src_frame_callback(
    P1GDisplayStreamSrc *self, CGDisplayStreamFrameStatus status, uint64_t displayTime,
    IOSurfaceRef frameSurface, CGDisplayStreamUpdateRef updateRef);
static GstFlowReturn p1g_display_stream_src_create(GstPushSrc *pushsrc, GstBuffer **outbuf);

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE(
    "src", GST_PAD_SRC, GST_PAD_ALWAYS, GST_STATIC_CAPS(
        "video/x-raw, "
            "format = (string) { BGRA }, "
            "width = (int) [ 1, max ], "
            "height = (int) [ 1, max ]"
    )
);


static void p1g_display_stream_src_class_init(P1GDisplayStreamSrcClass *klass)
{
    parent_class = g_type_class_ref(GST_TYPE_PUSH_SRC);

    GstPushSrcClass *pushsrc_class = GST_PUSH_SRC_CLASS(klass);
    pushsrc_class->create = p1g_display_stream_src_create;

    GstBaseSrcClass *basesrc_class = GST_BASE_SRC_CLASS(klass);
    basesrc_class->get_caps    = p1g_display_stream_src_get_caps;
    basesrc_class->start       = p1g_display_stream_src_start;
    basesrc_class->stop        = p1g_display_stream_src_stop;
    basesrc_class->unlock      = p1g_display_stream_src_unlock;
    basesrc_class->unlock_stop = p1g_display_stream_src_unlock_stop;

    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&src_template));
    gst_element_class_set_static_metadata(element_class, "P1stream display stream source",
                                           "Src/Video",
                                           "Captures frames from a display",
                                           "Stéphan Kochen <stephan@kochen.nl>");

    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->dispose = p1g_display_stream_src_dispose;
}

static void p1g_display_stream_src_init(P1GDisplayStreamSrc *self)
{
    self->display_stream = NULL;
    self->current_buffer = NULL;
    self->flushing = FALSE;
    g_cond_init(&self->cond);

    GstBaseSrc *basesrc = GST_BASE_SRC(self);
    gst_base_src_set_live(basesrc, TRUE);
    gst_base_src_set_format(basesrc, GST_FORMAT_BUFFERS);
}

static void p1g_display_stream_src_dispose(GObject* gobject)
{
    P1GDisplayStreamSrc *self = P1G_DISPLAY_STREAM_SRC(gobject);

    g_cond_clear(&self->cond);

    G_OBJECT_CLASS(parent_class)->dispose(gobject);
}

static GstCaps *p1g_display_stream_src_get_caps(GstBaseSrc *src, GstCaps *filter)
{
    const CGDirectDisplayID display_id = kCGDirectMainDisplay;
    size_t width  = CGDisplayPixelsWide(display_id);
    size_t height = CGDisplayPixelsHigh(display_id);
    return gst_caps_new_simple("video/x-raw",
                               "format", G_TYPE_STRING, "BGRA",
                               "width", G_TYPE_INT, width,
                               "height", G_TYPE_INT, height, NULL);
}

static gboolean p1g_display_stream_src_start(GstBaseSrc *basesrc)
{
    gboolean res = FALSE;
    P1GDisplayStreamSrc *self = P1G_DISPLAY_STREAM_SRC(basesrc);

    dispatch_queue_t global_queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0);
    const CGDirectDisplayID display_id = kCGDirectMainDisplay;
    size_t width  = CGDisplayPixelsWide(display_id);
    size_t height = CGDisplayPixelsHigh(display_id);

    self->display_stream = CGDisplayStreamCreateWithDispatchQueue(
        display_id, width, height, k32BGRAPixelFormat, NULL, global_queue,
        ^(CGDisplayStreamFrameStatus status, uint64_t displayTime, IOSurfaceRef frameSurface, CGDisplayStreamUpdateRef updateRef) {
            p1g_display_stream_src_frame_callback(self, status, displayTime, frameSurface, updateRef);
    });
    if (self->display_stream) {
        res = CGDisplayStreamStart(self->display_stream) == kCGErrorSuccess;
        if (!res) {
            CFRelease(self->display_stream);
            self->display_stream = NULL;
        }
    }

    return TRUE;
}

static gboolean p1g_display_stream_src_stop(GstBaseSrc *basesrc)
{
    gboolean res = FALSE;
    P1GDisplayStreamSrc *self = P1G_DISPLAY_STREAM_SRC(basesrc);
    GST_OBJECT_LOCK(self);

    if (CGDisplayStreamStop(self->display_stream) == kCGErrorSuccess) {
        // Join with the display stream before releasing.
        while (self->current_buffer)
            g_cond_wait(&self->cond, GST_OBJECT_GET_LOCK(self));

        CFRelease(self->display_stream);
        self->display_stream = NULL;

        res = TRUE;
    }
    
    GST_OBJECT_UNLOCK(self);
    return res;
}

static gboolean p1g_display_stream_src_unlock(GstBaseSrc *basesrc)
{
    P1GDisplayStreamSrc *self = P1G_DISPLAY_STREAM_SRC(basesrc);
    GST_OBJECT_LOCK(self);

    self->flushing = TRUE;
    g_cond_signal(&self->cond);

    GST_OBJECT_UNLOCK(self);    
    return TRUE;
}

static gboolean p1g_display_stream_src_unlock_stop(GstBaseSrc *basesrc)
{
    P1GDisplayStreamSrc *self = P1G_DISPLAY_STREAM_SRC(basesrc);
    GST_OBJECT_LOCK(self);

    self->flushing = FALSE;

    GST_OBJECT_UNLOCK(self);
    return TRUE;
}

static void p1g_display_stream_src_frame_callback(
    P1GDisplayStreamSrc *self, CGDisplayStreamFrameStatus status, uint64_t displayTime,
    IOSurfaceRef frameSurface, CGDisplayStreamUpdateRef updateRef)
{
    GST_OBJECT_LOCK(self);

    if (status == kCGDisplayStreamFrameStatusFrameComplete) {
        if (self->current_buffer)
            gst_buffer_unref(self->current_buffer);
        self->current_buffer = gst_buffer_new_with_iosurface(frameSurface, 0);
        g_cond_broadcast(&self->cond);
    }
    else if (status == kCGDisplayStreamFrameStatusStopped) {
        if (self->current_buffer) {
            gst_buffer_unref(self->current_buffer);
            self->current_buffer = NULL;
        }
        g_cond_broadcast(&self->cond);
    }

    GST_OBJECT_UNLOCK(self);
}

static GstFlowReturn p1g_display_stream_src_create(GstPushSrc *pushsrc, GstBuffer **outbuf)
{
    GstFlowReturn res;
    P1GDisplayStreamSrc *self = P1G_DISPLAY_STREAM_SRC(pushsrc);
    GST_OBJECT_LOCK(self);

    g_cond_wait(&self->cond, GST_OBJECT_GET_LOCK(self));
    if (self->flushing) {
        res = GST_FLOW_FLUSHING;
    }
    else if (self->current_buffer) {
        *outbuf = gst_buffer_ref(self->current_buffer);
        res = GST_FLOW_OK;
    }
    else {
        res = GST_FLOW_EOS;
    }

    GST_OBJECT_UNLOCK(self);
    return res;
}
