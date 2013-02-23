#include <gst/video/gstvideosink.h>
#import <QuartzCore/QuartzCore.h>

@class P1GPreview;


#define P1G_TYPE_PREVIEW_SINK  (p1g_preview_sink_get_type())
#define P1G_PREVIEW_SINK(obj)  (G_TYPE_CHECK_INSTANCE_CAST((obj), P1G_TYPE_PREVIEW_SINK, P1GPreviewSink))
#define P1G_PREVIEW_SINK_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST((klass), P1G_TYPE_PREVIEW_SINK, P1GPreviewSinkClass))
#define P1G_IS_PREVIEW_SINK(obj)  (G_TYPE_CHECK_INSTANCE_TYPE((obj), P1G_TYPE_PREVIEW_SINK))
#define P1G_IS_PREVIEW_SINK_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), P1G_TYPE_PREVIEW_SINK))
#define P1G_PREVIEW_SINK_GET_CLASS(klass)  (G_TYPE_INSTANCE_GET_CLASS((klass), P1G_TYPE_PREVIEW_SINK, P1GPreviewSinkClass))

typedef struct _P1GPreviewSink P1GPreviewSink;
typedef struct _P1GPreviewSinkClass P1GPreviewSinkClass;

struct _P1GPreviewSink
{
    GstVideoSink parent_instance;

    CFTypeRef viewRef;
};

struct _P1GPreviewSinkClass
{
    GstVideoSinkClass parent_class;
};

GType p1g_preview_sink_get_type();


@interface P1GPreview : NSView
{
    CGColorSpaceRef colorspace;
    NSLayoutConstraint *videoConstraint;

    struct P1GPreviewInfo {
        CGBitmapInfo bitmapInfo;
        gint width;
        gint height;
        gint stride;
    } info;

    GstBuffer *currentBuffer;
}

@property (nonatomic, readonly) P1GPreviewSink *element;

- (struct P1GPreviewInfo *)infoRef;

- (void)updateVideoConstraint;
- (void)clearVideoConstraint;

- (void)setBuffer:(GstBuffer *)buffer;

@end

