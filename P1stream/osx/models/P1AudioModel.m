#import "P1AudioModel.h"
#import "P1ListUtils.h"


@implementation P1AudioModel

- (id)initWithObject:(P1Object *)object
{
    self = [super initWithObject:object];
    if (self) {
        _sourceModels = [NSMutableArray new];
    }
    return self;
}


- (P1Audio *)audio
{
    return (P1Audio *)self.object;
}


- (P1ListNode *)sourcesHead
{
    return &self.audio->sources;
}

- (void)resync
{
    p1_audio_resync(self.audio);
}


- (NSArray *)sourceModels
{
    return _sourceModels;
}

- (void)insertObject:(P1ObjectModel *)objectModel inSourceModelsAtIndex:(NSUInteger)index
{
    P1Source *src = (P1Source *)objectModel.object;

    [_sourceModels insertObject:objectModel atIndex:index];

    [self lock];
    insertSourceInListAtIndex(self.sourcesHead, src, index);
    [self resync];
    [self unlock];
}

- (void)removeObjectFromSourceModelsAtIndex:(NSUInteger)index
{
    [_sourceModels removeObjectAtIndex:index];

    [self lock];
    removeSourceFromListAtIndex(self.sourcesHead, index);
    [self resync];
    [self unlock];
}

@end
