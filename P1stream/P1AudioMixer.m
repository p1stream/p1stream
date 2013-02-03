#import "P1AudioMixer.h"
#import "P1InputAudioSource.h"


@implementation P1AudioSourceSlot

@synthesize source, volume;

- (id)initForSource:(id<P1AudioSource>)source_ withVolume:(Float32)volume_
{
    self = [super init];
    if (self) {
        source = source_;
        volume = volume_;
    }
    return self;
}

@end


@implementation P1AudioMixer

@synthesize slots;

- (id)init
{
    self = [super init];
    if (self) {
        slots = [[NSMutableArray alloc] init];
    }
    return self;
}

- (P1AudioSourceSlot *)addSource:(id<P1AudioSource>)source withVolume:(Float32)volume
{
    @synchronized(self) {
        P1AudioSourceSlot *slot = [[P1AudioSourceSlot alloc] initForSource:source
                                                                withVolume:volume];
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
    // Determine a new clock source.
    // FIXME: More intelligent logic? User choice?
    id<P1AudioSource> newSource = nil;
    if ([slots count] != 0) {
        P1AudioSourceSlot *slot = [slots objectAtIndex:0];
        newSource = slot.source;
    }
    // FIXME: dummy clock source if nil.

    // Swap clock source.
    if (newSource != clockSource) {
        if (clockSource) {
            [clockSource setDelegate:nil];
        }
        clockSource = newSource;
        if (clockSource) {
            [clockSource setDelegate:self];
        }
    }
}

- (void)audioSourceClockTick
{
    dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);

    // No use if we don't have a delegate.
    id delegate_ = [self delegate];
    if (!delegate_) return;

    @synchronized(self) {
        Float32 *outputBuffer = [delegate_ getAudioMixerOutputBuffer:P1_AUDIO_MIXER_BUFFER_SIZE];
        if (!outputBuffer) return;

        // Mix buffers.
        memset(outputBuffer, 0, P1_AUDIO_MIXER_BUFFER_SIZE);
        for (P1AudioSourceSlot *slot in slots) {
            id<P1AudioSource> source = slot.source;
            Float32 *buffer = [source getCurrentBufferLocked];
            if (buffer) {
                for (size_t i = 0; i < P1_AUDIO_MIXER_BUFFER_SIZE_FLOATS; i++)
                    outputBuffer[i] += buffer[i];
                [source unlockBuffer:buffer];
            }
        }
    }

    // Callback async.
    dispatch_async(queue, ^{
        [delegate_ audioMixerBufferReady];
    });
}

- (NSDictionary *)serialize
{
    @synchronized(self) {
        return @{
            @"slots" : [slots mapObjectsWithBlock:^NSDictionary *(P1AudioSourceSlot *slot, NSUInteger idx) {
                id<P1AudioSource> source = slot.source;

                NSString *name;
                Class sourceClass = [source class];
                if (sourceClass == [P1InputAudioSource class])
                    name = @"input";

                return @{
                    @"type" : name,
                    @"volume" : [NSNumber numberWithFloat:slot.volume],
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
            if ([name isEqualToString:@"input"])
                sourceClass = [P1InputAudioSource class];
            else
                continue;

            id<P1AudioSource> source = [[sourceClass alloc] init];
            [source deserialize:[slotDict objectForKey:@"params"]];

            Float32 volume = [[slotDict objectForKey:@"volume"] floatValue];

            [self addSource:source withVolume:volume];
        }
    }
}

@end
