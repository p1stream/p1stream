#import "P1Preview.h"


@interface P1Pipeline : NSObject
{
    GstElement *pipeline, *source, *upload, *render, *download,
        *tee, *upload2, *preview, *convert, *x264enc, *flvmux, *rtmp;
}

- (id)initWithPreview:(P1Preview *)preview;

- (void)start;
- (void)stop;

@end
