#define P1_AUDIO_MIXER_BUFFER_SIZE_FLOATS (size_t)(44100 * 2 * 0.5f)
#define P1_AUDIO_MIXER_BUFFER_SIZE (P1_AUDIO_MIXER_BUFFER_SIZE_FLOATS * 32 / 8)

@class P1AudioSourceSlot;


@protocol P1AudioSourceDelegate <NSObject>

@required
- (void)audioSourceClockTick;

@end


@protocol P1AudioSource <NSObject>

// Audio mixer slot.
@required
- (void)setSlot:(P1AudioSourceSlot *)slot;

// Buffer access.
- (Float32 *)getCurrentBufferLocked;
- (void)unlockBuffer:(Float32 *)buffer;

// Clock source.
@optional
- (void)setDelegate:(id)delegate;

// Serialization.
@required
- (NSDictionary *)serialize;
- (void)deserialize:(NSDictionary *)dict;

@end


@interface P1AudioSourceSlot : NSObject

@property (retain, nonatomic, readonly) id<P1AudioSource> source;
@property Float32 volume;

- (id)initForSource:(id<P1AudioSource>)source withVolume:(Float32)volume;

@end


@protocol P1AudioMixerDelegate <NSObject>

@required
- (void *)getAudioMixerOutputBuffer:(size_t)size;
- (void)audioMixerBufferReady;

@end


// The canvas combines audio sources into a single stream.
@interface P1AudioMixer : NSObject <P1AudioSourceDelegate>
{
    id<P1AudioSource> clockSource;
}

@property (retain, readonly) NSMutableArray *slots;
@property (retain) id delegate;

- (P1AudioSourceSlot *)addSource:(id<P1AudioSource>)source withVolume:(Float32)volume;
- (void)removeAllSources;

- (NSDictionary *)serialize;
- (void)deserialize:(NSDictionary *)dict;

@end
