#import "P1GPreview.h"


G_DEFINE_TYPE(P1GPreviewSink, p1g_preview_sink, GST_TYPE_VIDEO_SINK)
static GstVideoSinkClass *parent_class = NULL;

static GstFlowReturn p1g_preview_sink_show_frame(GstVideoSink *video_sink, GstBuffer *buf);
static gboolean p1g_preview_sink_set_caps(GstBaseSink *sink, GstCaps *caps);
static GstStateChangeReturn p1g_preview_sink_change_state(GstElement *element, GstStateChange transition);

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE(
    "sink", GST_PAD_SINK, GST_PAD_ALWAYS, GST_STATIC_CAPS(
        "video/x-raw, "
            "format = (string) { BGRA, ABGR, RGBA, ARGB, BGRx, xBGR, RGBx, xRGB }, "
            "width = (int) [ 1, max ], "
            "height = (int) [ 1, max ], "
            "framerate = (fraction) [ 0, max ]"
    )
);


static void p1g_preview_sink_class_init(P1GPreviewSinkClass *klass)
{
    parent_class = g_type_class_ref(GST_TYPE_VIDEO_SINK);

    GstVideoSinkClass *videosink_class = GST_VIDEO_SINK_CLASS(klass);
    videosink_class->show_frame = p1g_preview_sink_show_frame;

    GstBaseSinkClass *basesink_class = GST_BASE_SINK_CLASS(klass);
    basesink_class->set_caps = p1g_preview_sink_set_caps;

    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
    element_class->change_state = p1g_preview_sink_change_state;
    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&sink_template));

    gst_element_class_set_static_metadata(element_class, "P1stream preview sink",
                                           "Sink/Video",
                                           "The P1stream video preview",
                                           "Stéphan Kochen <stephan@kochen.nl>");

}

static void p1g_preview_sink_init(P1GPreviewSink *self)
{
    self->viewRef = NULL;
}

static GstFlowReturn p1g_preview_sink_show_frame(GstVideoSink *videosink, GstBuffer *buf)
{
    P1GPreviewSink *self = P1G_PREVIEW_SINK(videosink);
    P1GPreview *view = (__bridge P1GPreview *)self->viewRef;

    view.buffer = buf;

    return GST_FLOW_OK;
}

static gboolean p1g_preview_sink_set_caps(GstBaseSink *basesink, GstCaps *caps)
{
    P1GPreviewSink *self = P1G_PREVIEW_SINK(basesink);
    P1GPreview *view = (__bridge P1GPreview *)self->viewRef;
    struct P1GPreviewInfo *info = [view infoRef];

    GstStructure *structure = gst_caps_get_structure(caps, 0);

    if (!gst_structure_get_int(structure, "width",  &info->width))
        return FALSE;
    if (!gst_structure_get_int(structure, "height", &info->height))
        return FALSE;
    info->stride = info->width * 4;

    const gchar *format = gst_structure_get_string(structure, "format");
    if (g_strcmp0(format, "BGRA"))
        info->bitmapInfo = kCGBitmapByteOrder32Little | kCGImageAlphaFirst;
    else if (g_strcmp0(format, "ABGR"))
        info->bitmapInfo = kCGBitmapByteOrder32Little | kCGImageAlphaLast;
    else if (g_strcmp0(format, "RGBA"))
        info->bitmapInfo = kCGBitmapByteOrder32Big | kCGImageAlphaLast;
    else if (g_strcmp0(format, "ARGB"))
        info->bitmapInfo = kCGBitmapByteOrder32Big | kCGImageAlphaFirst;
    else if (g_strcmp0(format, "BGRx"))
        info->bitmapInfo = kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst;
    else if (g_strcmp0(format, "xBGR"))
        info->bitmapInfo = kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipLast;
    else if (g_strcmp0(format, "RGBx"))
        info->bitmapInfo = kCGBitmapByteOrder32Big | kCGImageAlphaNoneSkipLast;
    else if (g_strcmp0(format, "xRGB"))
        info->bitmapInfo = kCGBitmapByteOrder32Big | kCGImageAlphaNoneSkipFirst;
    else
        return FALSE;

    return TRUE;
}

static GstStateChangeReturn p1g_preview_sink_change_state(GstElement *element, GstStateChange transition)
{
    GstStateChangeReturn res;
    P1GPreviewSink *self = P1G_PREVIEW_SINK(element);
    P1GPreview *view = (__bridge P1GPreview *)self->viewRef;

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

    switch (transition) {
        case GST_STATE_CHANGE_READY_TO_NULL:
            view.buffer = NULL;
            CFRelease(self->viewRef);
            break;
        default:
            break;
    }

    return res;
}


@implementation P1GPreview

@synthesize element;

- (id)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    if (self) {
        CALayer *layer = [CALayer layer];
        colorspace = CGColorSpaceCreateDeviceRGB();
        element = g_object_new(P1G_TYPE_PREVIEW_SINK, NULL);
        if (!layer || !colorspace || !element)
            return nil;

        self.layer = layer;
        self.wantsLayer = TRUE;
        layer.delegate = self;
        layer.backgroundColor = CGColorGetConstantColor(kCGColorBlack);
        layer.cornerRadius = 5;
        layer.masksToBounds = TRUE;
        layer.opaque = TRUE;

        element->viewRef = (__bridge CFTypeRef)self;
    }
    return self;
}

- (void)dealloc
{
    if (element)
        g_object_unref(element);
    if (colorspace)
        CFRelease(colorspace);
}

- (struct P1GPreviewInfo *)infoRef
{
    return &info;
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
        [self.layer performSelectorOnMainThread:@selector(setNeedsDisplay)
                                     withObject:nil
                                  waitUntilDone:FALSE];
    }
}

- (BOOL)mouseDownCanMoveWindow
{
    return TRUE;
}

- (void)drawLayer:(CALayer *)layer inContext:(CGContextRef)ctx
{
    GstBuffer *buffer = NULL;
    @synchronized(self) {
        if (currentBuffer)
            buffer = gst_buffer_ref(currentBuffer);
    }

    GstMapInfo mapinfo;
    gboolean mapped = FALSE;
    if (buffer)
        mapped = gst_buffer_map(buffer, &mapinfo, GST_MAP_READ);

    CGDataProviderRef provider = NULL;
    if (mapped)
        provider = CGDataProviderCreateWithData(NULL, mapinfo.data, mapinfo.size, NULL);

    CGImageRef image = NULL;
    if (provider) {
        image = CGImageCreate(info.width, info.height, 8, 32,
                              info.stride, colorspace, info.bitmapInfo,
                              provider, NULL, TRUE, kCGRenderingIntentDefault);
        CFRelease(provider);
    }

    if (image) {
        CGContextDrawImage(ctx, layer.bounds, image);
        CFRelease(image);
    }
    
    if (mapped)
        gst_buffer_unmap(buffer, &mapinfo);

    if (buffer)
        gst_buffer_unref(buffer);
}

@end
