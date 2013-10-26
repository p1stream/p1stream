#import "P1PluginModel.h"


@interface P1AudioModel : P1ObjectModel

- (id)initWithContext:(P1Context *)context;

- (void)enumerateSourceModels:(void (^)(P1PluginModel *sourceModel))block;

- (void)reconfigurePlugins;

@end
