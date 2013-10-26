#import "P1ObjectModel.h"


@interface P1PluginModel : P1ObjectModel

@property (retain) NSString *uuid;

@property (readonly) BOOL isPendingRemoval;

- (id)initWithObject:(P1Object *)object
                name:(NSString *)name
                uuid:(NSString *)uuid;

- (void)remove;
- (void)destroy;

@end
