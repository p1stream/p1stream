#import <QuartzCore/QuartzCore.h>
#include <gst/video/gstvideosink.h>

@class P1GCALayer;


#define P1G_TYPE_CALAYER_SINK  (p1g_calayer_sink_get_type())
#define P1G_CALAYER_SINK(obj)  (G_TYPE_CHECK_INSTANCE_CAST((obj), P1G_TYPE_CALAYER_SINK, P1GCALayerSink))
#define P1G_CALAYER_SINK_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST((klass), P1G_TYPE_CALAYER_SINK, P1GCALayerSinkClass))
#define P1G_IS_CALAYER_SINK(obj)  (G_TYPE_CHECK_INSTANCE_TYPE((obj), P1G_TYPE_CALAYER_SINK))
#define P1G_IS_CALAYER_SINK_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), P1G_TYPE_CALAYER_SINK))
#define P1G_CALAYER_SINK_GET_CLASS(klass)  (G_TYPE_INSTANCE_GET_CLASS((klass), P1G_TYPE_CALAYER_SINK, P1GCALayerSinkClass))

typedef struct _P1GCALayerSink P1GCALayerSink;
typedef struct _P1GCALayerSinkClass P1GCALayerSinkClass;

struct _P1GCALayerSink
{
    GstVideoSink parent_instance;

    __unsafe_unretained CALayer *layer;

    /*< private >*/
    CGColorSpaceRef colorspace;
    CGBitmapInfo bitmapInfo;
    gint width;
    gint height;
    gint stride;
};

struct _P1GCALayerSinkClass
{
    GstVideoSinkClass parent_class;
};

GType p1g_calayer_sink_get_type();


// Keeps a GstBuffer referenced and mapped while alive.
@interface P1GBufferMapping : NSObject
{
    GstBuffer *buf;
    GstMapInfo mapinfo;
}

+ (id)mappingWithBuffer:(GstBuffer *)buf;
- (id)initWithBuffer:(GstBuffer *)buf;
- (guint8 *)data;
- (gsize)size;

@end
