#import "P1ObjectModel.h"


@interface P1ConnectionModel : P1ObjectModel

@property (readonly) NSString *stateMessage;

+ (NSSet *)keyPathsForValuesAffectingStateMessage;

@end
