#import "P1Pipeline.h"


@implementation P1Pipeline

- (id)initWithPreview:(P1Preview *)previewObj
{
    self = [super init];
    if (self) {
        preview = GST_ELEMENT(previewObj.element);
        if (preview)
            gst_object_ref(preview);

        pipeline = gst_pipeline_new("test");
        source   = gst_element_factory_make("displaystreamsrc", "source");
        upload1  = gst_element_factory_make("textureupload",    "upload1");
        render   = gst_element_factory_make("rendertextures",   "render");
        download = gst_element_factory_make("texturedownload",  "download");
        tee      = gst_element_factory_make("tee",              "tee");
        queue1   = gst_element_factory_make("queue",            "queue1");
        upload2  = gst_element_factory_make("textureupload",    "upload2");
        queue2   = gst_element_factory_make("queue",            "queue2");
        convert  = gst_element_factory_make("videoconvert",     "convert");
        x264enc  = gst_element_factory_make("x264enc",          "x264enc");
        flvmux   = gst_element_factory_make("flvmux",           "flvmux");
        queue3   = gst_element_factory_make("queue",            "queue3");
        rtmp     = gst_element_factory_make("rtmpsink",         "rtmp");
        g_assert(pipeline && source && upload1 && render && download && tee &&
            queue1 && upload2 && preview && queue2 && convert && x264enc && flvmux && queue3 && rtmp);

        gst_bin_add_many(GST_BIN(pipeline), source, upload1, render, download, tee,
            queue1, upload2, preview, queue2, convert, x264enc, flvmux, queue3, rtmp, NULL);

        gboolean success =
            gst_element_link_many(source, upload1, render, download, tee, NULL) &&
            gst_element_link_many(tee, queue1, upload2, preview, NULL) &&
            gst_element_link_many(tee, queue2, convert, x264enc, flvmux, queue3, rtmp, NULL);
        g_assert(success);

        GValue val = G_VALUE_INIT;

        g_value_init(&val, G_TYPE_STRING);
        g_value_set_static_string(&val, "rtmp://127.0.0.1/app/test");
        g_object_set_property(G_OBJECT(rtmp), "location", &val);
        g_value_unset(&val);

        g_value_init(&val, G_TYPE_INT);
        g_value_set_int(&val, 128 * 1024 * 1024);
        g_object_set_property(G_OBJECT(queue2), "max-size-bytes", &val);
        g_value_set_int(&val, 5);
        g_object_set_property(G_OBJECT(x264enc), "rc-lookahead", &val);
        g_value_unset(&val);

        g_value_init(&val, G_TYPE_BOOLEAN);
        g_value_set_boolean(&val, TRUE);
        g_object_set_property(G_OBJECT(queue1), "leaky", &val);
        g_object_set_property(G_OBJECT(queue2), "leaky", &val);
        g_object_set_property(G_OBJECT(flvmux), "streamable", &val);
        g_value_unset(&val);

        [self stop];
    }
    return self;
}

- (void)dealloc
{
    if (pipeline) {
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
    }
}

- (void)start
{
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
}

- (void)stop
{
    gst_element_set_state(pipeline, GST_STATE_READY);
}

@end
