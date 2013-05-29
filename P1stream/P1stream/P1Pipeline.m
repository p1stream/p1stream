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
        upload   = gst_element_factory_make("textureupload",    "upload");
        render   = gst_element_factory_make("rendertextures",   "render");
        download = gst_element_factory_make("texturedownload",  "download");
        tee      = gst_element_factory_make("tee",              "tee");
        upload2  = gst_element_factory_make("textureupload",    "upload2");
        convert  = gst_element_factory_make("videoconvert",     "convert");
        x264enc  = gst_element_factory_make("x264enc",          "x264enc");
        flvmux   = gst_element_factory_make("flvmux",           "flvmux");
        rtmp     = gst_element_factory_make("rtmpsink",         "rtmp");
        g_assert(pipeline && source && upload && render && download &&
            tee && upload2 && preview && convert && x264enc && flvmux && rtmp);

        gst_bin_add_many(GST_BIN(pipeline), source, upload, render, download,
            tee, upload2, preview, convert, x264enc, flvmux, rtmp, NULL);

        gboolean success =
            gst_element_link_many(source, upload, render, download, tee, NULL) &&
            gst_element_link_many(tee, upload2, preview, NULL) &&
            gst_element_link_many(tee, convert, x264enc, flvmux, rtmp, NULL);
        g_assert(success);

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
