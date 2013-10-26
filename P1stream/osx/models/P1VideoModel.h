#import "P1PluginModel.h"


@interface P1VideoModel : P1ObjectModel
{
    P1PluginModel *_newClock;
}

- (id)initWithContext:(P1Context *)context;

- (P1PluginModel *)clockModel;
- (void)enumerateSourceModels:(void (^)(P1PluginModel *sourceModel))block;

- (void)reconfigurePlugins;

- (BOOL)hasNewClockPending;
- (void)swapNewClock;

@end
