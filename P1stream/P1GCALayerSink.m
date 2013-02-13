#import "P1GCALayerSink.h"


G_DEFINE_TYPE(P1GCALayerSink, p1g_calayer_sink, GST_TYPE_VIDEO_SINK)

static GstCaps *p1g_calayer_get_caps(GstBaseSink *sink, GstCaps *filter);
static gboolean p1g_calayer_set_caps(GstBaseSink *sink, GstCaps *caps);
static GstFlowReturn p1g_calayer_sink_show_frame(GstVideoSink *video_sink, GstBuffer *buf);


static void p1g_calayer_sink_class_init(P1GCALayerSinkClass *klass)
{
    GstVideoSinkClass *videosink_class = GST_VIDEO_SINK_CLASS(klass);
    videosink_class->show_frame = p1g_calayer_sink_show_frame;

    GstBaseSinkClass *basesink_class = GST_BASE_SINK_CLASS(klass);
    basesink_class->get_caps = p1g_calayer_get_caps;
    basesink_class->set_caps = p1g_calayer_set_caps;
}

static void p1g_calayer_sink_init(P1GCALayerSink *self)
{
}

static GstCaps *p1g_calayer_get_caps(GstBaseSink *sink, GstCaps *filter)
{
    return NULL;
}

static gboolean p1g_calayer_set_caps(GstBaseSink *sink, GstCaps *caps)
{
    return FALSE;
}

static GstFlowReturn p1g_calayer_sink_show_frame(GstVideoSink *video_sink, GstBuffer *buf)
{
    return GST_FLOW_OK;
}
