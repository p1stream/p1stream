#import "P1Pipeline.h"
#import "P1DisplayStreamSrc.h"
#import "P1TextureUpload.h"
#import "P1RenderTextures.h"


@implementation P1Pipeline

- (id)initWithPreview:(P1Preview *)preview
{
    self = [super init];
    if (self) {
        sink = GST_ELEMENT(preview.element);
        if (sink)
            gst_object_ref(sink);

        pipeline = gst_pipeline_new("test");
        source = g_object_new(P1_TYPE_DISPLAY_STREAM_SRC, NULL);
        upload = g_object_new(P1_TYPE_TEXTURE_UPLOAD, NULL);
        render = g_object_new(P1_TYPE_RENDER_TEXTURES, NULL);
        g_assert(pipeline && source && upload && render && sink);

        gboolean success;
        gst_bin_add_many(GST_BIN(pipeline), source, upload, render, sink, NULL);
        success = gst_element_link_many(source, upload, render, sink, NULL);
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
