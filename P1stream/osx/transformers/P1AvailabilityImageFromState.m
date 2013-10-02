#import "P1AvailabilityImageFromState.h"


@implementation P1AvailabilityImageFromState

+ (Class)transformedValueClass
{
    return [NSImage class];
}

+ (BOOL)allowsReverseTransformation
{
    return NO;
}

- (id)transformedValue:(id)value
{
    P1State state = [value intValue];

    NSString *imageName;
    switch (state) {
        case P1_STATE_IDLE:
            imageName = NSImageNameStatusNone;
            break;
        case P1_STATE_STARTING:
            imageName = NSImageNameStatusPartiallyAvailable;
            break;
        case P1_STATE_RUNNING:
            imageName = NSImageNameStatusAvailable;
            break;
        case P1_STATE_STOPPING:
            imageName = NSImageNameStatusPartiallyAvailable;
            break;
        default:
            imageName = NSImageNameStatusUnavailable;
            break;
    }

    return [NSImage imageNamed:imageName];
}

@end
