#import "P1Pipeline.h"
#import "P1DisplayStreamSrc.h"
#import "P1TextureUpload.h"


@implementation P1Pipeline

- (id)initWithPreview:(P1Preview *)preview
{
    self = [super init];
    if (self) {
        sink = GST_ELEMENT(preview.element);
        if (sink)
            gst_object_ref(sink);

        pipeline = gst_pipeline_new("test");
        source = g_object_new(P1G_TYPE_DISPLAY_STREAM_SRC, NULL);
        upload = g_object_new(P1G_TYPE_TEXTURE_UPLOAD, NULL);
        if (!pipeline || !source || !upload || !sink) {
            [NSException raise:NSGenericException
                        format:@"Could not construct pipeline elements"];
        }

        gst_bin_add_many(GST_BIN(pipeline), source, upload, sink, NULL);
        gst_element_link_many(source, upload, sink, NULL);

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
