#import "P1Preview.h"


@interface P1Pipeline : NSObject
{
    GstElement *pipeline, *source, *upload1, *render, *tee, *queue1, *download1,
        *upload2, *preview, *queue2, *download2, *convert, *x264enc, *flvmux,
        *queue3, *rtmp;
}

- (id)initWithPreview:(P1Preview *)preview;

- (void)start;
- (void)stop;

@end
