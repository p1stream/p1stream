#import "P1Pipeline.h"
#import "P1DisplayStreamSrc.h"


@implementation P1Pipeline

- (id)initWithPreview:(P1Preview *)preview
{
    self = [super init];
    if (self) {
        sink = GST_ELEMENT(preview.element);
        if (sink)
            g_object_ref(sink);

        pipeline = gst_pipeline_new("preview-test");
        source   = g_object_new(P1G_TYPE_DISPLAY_STREAM_SRC, NULL);
        if (!pipeline || !source || !sink) {
            [NSException raise:NSGenericException
                        format:@"Could not construct pipeline elements"];
        }

        gst_bin_add_many(GST_BIN(pipeline), source, sink, NULL);
        gst_element_link_many(source, sink, NULL);
    }
    return self;
}

- (void)dealloc
{
    if (pipeline)
        [self stop];

    if (sink)
        g_object_unref(sink);

    if (source)
        g_object_unref(source);

    if (pipeline)
        g_object_unref(pipeline);
}

- (void)start
{
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
}

- (void)stop
{
    gst_element_set_state(pipeline, GST_STATE_NULL);
}

@end
