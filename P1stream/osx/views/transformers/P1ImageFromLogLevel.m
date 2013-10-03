#import "P1ImageFromLogLevel.h"


@implementation P1ImageFromLogLevel

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
    P1LogLevel level = [value intValue];
    switch (level) {
        case P1_LOG_WARNING:
            return [NSImage imageNamed:NSImageNameStatusPartiallyAvailable];
        case P1_LOG_ERROR:
            return [NSImage imageNamed:NSImageNameStatusUnavailable];
        default:
            return nil;
    }
}

@end
