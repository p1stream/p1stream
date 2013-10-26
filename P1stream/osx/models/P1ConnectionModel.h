#import "P1ObjectModel.h"


@interface P1ConnectionModel : P1ObjectModel

@property (readonly) NSString *stateMessage;

- (id)initWithContext:(P1Context *)context;

+ (NSSet *)keyPathsForValuesAffectingStateMessage;

@end
