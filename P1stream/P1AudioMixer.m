#import "P1AudioMixer.h"
#import "P1HALAudioSource.h"


@implementation P1AudioSourceSlot

@synthesize source;

- (id)initForSource:(id<P1AudioSource>)source_
{
    self = [super init];
    if (self) {
        source = source_;
    }
    return self;
}

@end


@implementation P1AudioMixer

@synthesize slots;

- (P1AudioSourceSlot *)addSource:(id<P1AudioSource>)source
{
    @synchronized(self) {
        P1AudioSourceSlot *slot = [[P1AudioSourceSlot alloc] initForSource:source];
        if (!slot) {
            NSLog(@"Failed to create slot for new audio source.");
            return nil;
        }
        [source setSlot:slot];
        [slots addObject:slot];

        [self updateSourceState];

        return slot;
    }
}

- (void)removeAllSources
{
    @synchronized(self) {
        for (P1AudioSourceSlot *slot in slots) {
            [slot.source setSlot:nil];
        }
        [slots removeAllObjects];

        [self updateSourceState];
    }
}

// Private. Sources were updated, refresh state.
- (void)updateSourceState
{
    // FIXME
}

- (NSDictionary *)serialize
{
    @synchronized(self) {
        return @{
            @"slots" : [slots mapObjectsWithBlock:^NSDictionary *(P1AudioSourceSlot *slot, NSUInteger idx) {
                id<P1AudioSource> source = slot.source;

                NSString *name;
                Class sourceClass = [source class];
                if (sourceClass == [P1HALAudioSource class])
                    name = @"hal";

                return @{
                    @"type" : name,
                    @"params" : [source serialize]
                };
            }]
        };
    }
}

- (void)deserialize:(NSDictionary *)dict
{
    @synchronized(self) {
        [self removeAllSources];
        for (NSDictionary *slotDict in [dict objectForKey:@"slots"]) {
            Class sourceClass;
            NSString *name = [slotDict objectForKey:@"type"];
            if ([name isEqualToString:@"hal"])
                sourceClass = [P1HALAudioSource class];
            else
                continue;

            id<P1AudioSource> source = [[sourceClass alloc] init];
            [source deserialize:[slotDict objectForKey:@"params"]];

            [self addSource:source];
        }
    }
}

@end
