#import "P1ObjectModel.h"


@interface P1LogMessage : NSObject

@property (readonly, retain) P1ObjectModel *model;
@property (readonly, assign) P1LogLevel level;
@property (readonly, retain) NSString *message;

- (id)initWithModel:(P1ObjectModel *)model
           andLevel:(P1LogLevel)level
         andMessage:(NSString *)message;

@end
