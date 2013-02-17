#import "P1GCALayerSink.h"


G_DEFINE_TYPE(P1GCALayerSink, p1g_calayer_sink, GST_TYPE_VIDEO_SINK)
static GstVideoSinkClass *parent_class = NULL;

static gboolean p1g_calayer_sink_set_caps(GstBaseSink *sink, GstCaps *caps);
static GstFlowReturn p1g_calayer_sink_show_frame(GstVideoSink *video_sink, GstBuffer *buf);
static GstStateChangeReturn p1g_calayer_sink_change_state(GstElement *element, GstStateChange transition);

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE(
    "sink", GST_PAD_SINK, GST_PAD_ALWAYS, GST_STATIC_CAPS(
        "video/x-raw, "
            "format = (string) { BGRA, ABGR, RGBA, ARGB, BGRx, xBGR, RGBx, xRGB }, "
            "width = (int) [ 1, max ], "
            "height = (int) [ 1, max ], "
            "framerate = (fraction) [ 0, max ]"
    )
);


static void p1g_calayer_sink_class_init(P1GCALayerSinkClass *klass)
{
    parent_class = g_type_class_ref(GST_TYPE_VIDEO_SINK);

    GstVideoSinkClass *videosink_class = GST_VIDEO_SINK_CLASS(klass);
    videosink_class->show_frame = p1g_calayer_sink_show_frame;

    GstBaseSinkClass *basesink_class = GST_BASE_SINK_CLASS(klass);
    basesink_class->set_caps = p1g_calayer_sink_set_caps;

    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
    element_class->change_state = p1g_calayer_sink_change_state;
    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&sink_template));
}

static void p1g_calayer_sink_init(P1GCALayerSink *self)
{
    self->layer = NULL;
}

static gboolean p1g_calayer_sink_set_caps(GstBaseSink *basesink, GstCaps *caps)
{
    P1GCALayerSink *self = P1G_CALAYER_SINK(basesink);

    GstStructure *structure = gst_caps_get_structure(caps, 0);

    if (!gst_structure_get_int(structure, "width",  &self->width))
        return FALSE;
    if (!gst_structure_get_int(structure, "height", &self->height))
        return FALSE;
    self->stride = self->width * 4;

    const gchar *format = gst_structure_get_string(structure, "format");
    if (g_strcmp0(format, "BGRA"))
        self->bitmapInfo = kCGBitmapByteOrder32Little | kCGImageAlphaFirst;
    else if (g_strcmp0(format, "ABGR"))
        self->bitmapInfo = kCGBitmapByteOrder32Little | kCGImageAlphaLast;
    else if (g_strcmp0(format, "RGBA"))
        self->bitmapInfo = kCGBitmapByteOrder32Big | kCGImageAlphaLast;
    else if (g_strcmp0(format, "ARGB"))
        self->bitmapInfo = kCGBitmapByteOrder32Big | kCGImageAlphaFirst;
    else if (g_strcmp0(format, "BGRx"))
        self->bitmapInfo = kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst;
    else if (g_strcmp0(format, "xBGR"))
        self->bitmapInfo = kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipLast;
    else if (g_strcmp0(format, "RGBx"))
        self->bitmapInfo = kCGBitmapByteOrder32Big | kCGImageAlphaNoneSkipLast;
    else if (g_strcmp0(format, "xRGB"))
        self->bitmapInfo = kCGBitmapByteOrder32Big | kCGImageAlphaNoneSkipFirst;
    else
        return FALSE;

    return TRUE;
}

static void release_mapping(void *info, const void *data, size_t size)
{
    CFRelease(info);
}

static GstFlowReturn p1g_calayer_sink_show_frame(GstVideoSink *videosink, GstBuffer *buf)
{
    P1GCALayerSink *self = P1G_CALAYER_SINK(videosink);

    P1GBufferMapping *mapping = [P1GBufferMapping mappingWithBuffer:buf];
    if (!mapping)
        return GST_FLOW_ERROR;

    void *ref = (void *)CFBridgingRetain(mapping);
    CGDataProviderRef provider = CGDataProviderCreateWithData(ref, mapping.data, mapping.size, release_mapping);
    if (!provider) {
        CFRelease(ref);
        return GST_FLOW_ERROR;
    }

    CGImageRef image = CGImageCreate(self->width, self->height, 8, 32,
                                     self->stride, self->colorspace, self->bitmapInfo,
                                     provider, NULL, TRUE, kCGRenderingIntentDefault);
    CFRelease(provider);
    if (!image) {
        return GST_FLOW_ERROR;
    }

    self->layer.contents = CFBridgingRelease(image);
    return GST_FLOW_OK;
}

static GstStateChangeReturn p1g_calayer_sink_change_state(GstElement *element, GstStateChange transition)
{
    GstStateChangeReturn res;
    P1GCALayerSink *self = P1G_CALAYER_SINK(element);

    switch (transition) {
        case GST_STATE_CHANGE_NULL_TO_READY:
            self->colorspace = CGColorSpaceCreateDeviceRGB();
            if (!self->colorspace)
                return GST_STATE_CHANGE_FAILURE;
            break;
        default:
            break;
    }

    res = GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);

    switch (transition) {
        case GST_STATE_CHANGE_READY_TO_NULL:
            CFRelease(self->colorspace);
            break;
        default:
            break;
    }

    return res;
}


@implementation P1GBufferMapping

+ (id)mappingWithBuffer:(GstBuffer *)buf
{
    return [[self alloc] initWithBuffer:buf];
}

- (id)initWithBuffer:(GstBuffer *)buf_
{
    self = [super init];
    if (self) {
        if (!gst_buffer_map(buf_, &mapinfo, GST_MAP_READ))
            return nil;
        gst_buffer_ref(buf_);
        buf = buf_;
    }
    return self;
}

- (guint8 *)data
{
    return mapinfo.data;
}

- (gsize)size
{
    return mapinfo.size;
}

- (void)dealloc
{
    gst_buffer_unmap(buf, &mapinfo);
    gst_buffer_unref(buf);
}

@end
