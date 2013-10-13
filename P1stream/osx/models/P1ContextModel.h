#import "P1ObjectModel.h"
#import "P1AudioModel.h"
#import "P1VideoModel.h"
#import "P1ConnectionModel.h"
#import "P1LogMessage.h"


@interface P1ContextModel : P1ObjectModel
{
    NSFileHandle *_contextFileHandle;
    NSMutableArray *_logMessages;

    BOOL _restart;
    BOOL _needsConnectionRestart;
}

@property (readonly) P1Context *context;

@property (readonly, retain) NSArray *logMessages;

@property (readonly, retain) P1AudioModel* audioModel;
@property (readonly, retain) P1VideoModel* videoModel;
@property (readonly, retain) P1ConnectionModel* connectionModel;

@property (readonly) BOOL needsConnectionRestart;

- (void)reconfigure;

- (void)start;
- (void)stop;

@end
