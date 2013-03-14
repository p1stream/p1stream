#import "P1Preview.h"


@interface P1Pipeline : NSObject
{
    GstElement *pipeline, *source, *upload, *sink;
}

@property (retain, nonatomic, readonly) CALayer *layer;

- (id)initWithPreview:(P1Preview *)preview;

- (void)start;
- (void)stop;

@end
