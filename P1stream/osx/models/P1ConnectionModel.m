#import "P1ConnectionModel.h"


@implementation P1ConnectionModel

- (NSString *)stateMessage
{
    if (self.error) {
        return @"Connection failed";
    }
    else {
        switch (self.currentState) {
            case P1_STATE_IDLE:
                return @"Not connected";
            case P1_STATE_STARTING:
                return @"Connecting...";
            case P1_STATE_RUNNING:
                return @"Connected";
            case P1_STATE_STOPPING:
                return @"Closing connection...";
            default:
                return @"Unknown";
        }
    }
}
+ (NSSet *)keyPathsForValuesAffectingStateMessage
{
    return [NSSet setWithObjects:@"error", @"currentState", nil];
}

@end
