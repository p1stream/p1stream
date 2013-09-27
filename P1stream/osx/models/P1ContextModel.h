#import "P1ObjectModel.h"


@interface P1ContextModel : P1ObjectModel
{
    NSFileHandle *_contextFileHandle;

    // FIXME: This is a stub until we create proper ObjC models for stuff.
    // For now, we just create a fairly useless P1ObjectModel instance for every
    // P1Object, and reference it from here.
    NSMutableArray *_objects;
}

@property (readonly) P1Context *context;

- (void)start;
- (void)stop;

@end
