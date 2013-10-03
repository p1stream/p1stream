#import "P1ConnectionStringFromState.h"


@implementation P1ConnectionStringFromState

+ (Class)transformedValueClass
{
    return [NSString class];
}

+ (BOOL)allowsReverseTransformation
{
    return NO;
}

- (id)transformedValue:(id)value
{
    P1State state = [value intValue];
    switch (state) {
        case P1_STATE_IDLE:
            return @"Not connected";
        case P1_STATE_STARTING:
            return @"Connecting...";
        case P1_STATE_RUNNING:
            return @"Connected";
        case P1_STATE_STOPPING:
            return @"Closing connection...";
        default:
            return @"Connection failed";
    }
}

@end
