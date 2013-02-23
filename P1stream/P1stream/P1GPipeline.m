#import "P1GPipeline.h"


@implementation P1GPipeline

- (id)initWithPreview:(P1GPreview *)preview
{
    self = [super init];
    if (self) {
        sink = GST_ELEMENT(preview.element);
        if (sink)
            g_object_ref(sink);

        pipeline = gst_pipeline_new("preview-test");
        source   = gst_element_factory_make("videotestsrc", "videotestsrc");
        convert  = gst_element_factory_make("videoconvert", "videoconvert");
        if (!pipeline || !source || !convert || !sink) {
            [NSException raise:NSGenericException
                        format:@"Could not construct pipeline elements"];
        }

        gst_bin_add_many(GST_BIN(pipeline), source, convert, sink, NULL);
        gst_element_link_many(source, convert, sink, NULL);
    }
    return self;
}

- (void)dealloc
{
    if (pipeline)
        [self stop];

    if (sink)
        g_object_unref(sink);

    if (convert)
        g_object_unref(convert);

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
