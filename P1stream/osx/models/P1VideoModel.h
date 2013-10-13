#import "P1ObjectModel.h"


@interface P1VideoModel : P1ObjectModel
{
    P1ObjectModel *_clockModel;
    NSMutableArray *_sourceModels;
}

@property (readonly) P1Video *video;

@property (retain) P1ObjectModel *clockModel;

@property (readonly, retain) NSArray *sourceModels;
- (void)insertObject:(P1ObjectModel *)objectModel inSourceModelsAtIndex:(NSUInteger)index;
- (void)removeObjectFromSourceModelsAtIndex:(NSUInteger)index;

@end
