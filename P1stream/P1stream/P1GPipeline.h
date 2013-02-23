#import "P1GPreview.h"


@interface P1GPipeline : NSObject
{
    GstElement *pipeline, *source, *convert, *sink;
}

@property (retain, nonatomic, readonly) CALayer *layer;

- (id)initWithPreview:(P1GPreview *)preview;

- (void)start;
- (void)stop;

@end
