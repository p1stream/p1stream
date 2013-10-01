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

@end
