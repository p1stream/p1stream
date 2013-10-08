#import "P1LogMessage.h"


@implementation P1LogMessage

- (id)initWithModel:(P1ObjectModel *)model
           andLevel:(P1LogLevel)level
         andMessage:(NSString *)message
{
    self = [super init];
    if (self) {
        _model = model;
        _level = level;
        _message = message;
    }
    return self;
}

- (NSImage *)levelIcon
{
    switch (_level) {
        case P1_LOG_WARNING:
            return [NSImage imageNamed:NSImageNameStatusPartiallyAvailable];
        case P1_LOG_ERROR:
            return [NSImage imageNamed:NSImageNameStatusUnavailable];
        default:
            return nil;
    }
}
+ (NSSet *)keyPathsForValuesAffectingLevelIcon
{
    return [NSSet setWithObjects:@"level", nil];
}

@end
