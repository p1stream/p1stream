@class P1AudioSourceSlot;


@protocol P1AudioSource <NSObject>

// Audio mixer slot.
@required
- (void)setSlot:(P1AudioSourceSlot *)slot;

// Serialization.
@required
- (NSDictionary *)serialize;
- (void)deserialize:(NSDictionary *)dict;

@end


@interface P1AudioSourceSlot : NSObject

@property (retain, nonatomic, readonly) id<P1AudioSource> source;

- (id)initForSource:(id<P1AudioSource>)source;

@end


// The canvas combines audio sources into a single image.
@interface P1AudioMixer : NSObject

@property (retain, readonly) NSMutableArray *slots;

- (P1AudioSourceSlot *)addSource:(id<P1AudioSource>)source;
- (void)removeAllSources;

- (NSDictionary *)serialize;
- (void)deserialize:(NSDictionary *)dict;

@end
