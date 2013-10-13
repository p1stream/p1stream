#import "P1VideoModel.h"
#import "P1ListUtils.h"


@implementation P1VideoModel

- (id)initWithObject:(P1Object *)object
{
    self = [super initWithObject:object];
    if (self) {
        _sourceModels = [NSMutableArray new];
    }
    return self;
}


- (P1Video *)video
{
    return (P1Video *)self.object;
}


- (P1ListNode *)sourcesHead
{
    return &self.video->sources;
}

- (void)resync
{
    p1_video_resync(self.video);
}


- (P1ObjectModel *)clockModel
{
    return _clockModel;
}

- (void)setClockModel:(P1ObjectModel *)clockModel
{
    P1VideoClock *vclock = (P1VideoClock *)clockModel.object;

    _clockModel = clockModel;

    [self lock];
    self.video->clock = vclock;
    [self resync];
    [self unlock];
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
