#import "P1ObjectModel.h"


@interface P1AudioModel : P1ObjectModel
{
    NSMutableArray *_sourceModels;
}

@property (readonly) P1Audio *audio;

@property (readonly, retain) NSArray *sourceModels;
- (void)insertObject:(P1ObjectModel *)objectModel inSourceModelsAtIndex:(NSUInteger)index;
- (void)removeObjectFromSourceModelsAtIndex:(NSUInteger)index;

@end
