#import "P1Preview.h"


@interface P1Pipeline : NSObject
{
    GstElement *pipeline, *source, *upload1, *render, *download, *tee,
        *queue1, *upload2, *preview, *queue2, *convert, *x264enc, *flvmux, *rtmp;
}

- (id)initWithPreview:(P1Preview *)preview;

- (void)start;
- (void)stop;

@end
