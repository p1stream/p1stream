#import "P1IsRunningTarget.h"


@implementation P1IsRunningTarget

+ (Class)transformedValueClass
{
    return [NSNumber class];
}

+ (BOOL)allowsReverseTransformation
{
    return YES;
}

- (id)transformedValue:(id)value
{
    P1TargetState target = [value intValue];
    return @(target == P1_TARGET_RUNNING);
}

- (id)reverseTransformedValue:(id)value
{
    BOOL isRunning = [value intValue];
    return @(isRunning ? P1_TARGET_RUNNING : P1_TARGET_IDLE);
}

@end
