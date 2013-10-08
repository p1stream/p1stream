#import "P1ObjectModel.h"
#import "P1ConnectionModel.h"
#import "P1LogMessage.h"


@interface P1ContextModel : P1ObjectModel
{
    NSFileHandle *_contextFileHandle;
    NSMutableArray *_logMessages;

    // FIXME: This is a stub until we create proper ObjC models for plugins.
    // For now, we just create a fairly useless P1ObjectModel instance for every
    // P1Object, and reference it from here.
    NSMutableArray *_objects;
}

@property (readonly, assign) P1Context *context;

@property (readonly, retain) NSArray *logMessages;

@property (readonly, retain) P1ObjectModel* audioModel;
@property (readonly, retain) P1ObjectModel* videoModel;
@property (readonly, retain) P1ConnectionModel* connectionModel;

- (void)start;
- (void)stop;

@end
