@class P1AudioMixer;


@protocol P1AudioSource <NSObject>

@required
- (id)initWithMixer:(P1AudioMixer *)mixer;

// Property list serialization.
@required
- (NSDictionary *)serialize;
- (void)deserialize:(NSDictionary *)dict;

@end


// The canvas combines audio sources into a single image.
@interface P1AudioMixer : NSObject

@property (retain, readonly) NSMutableArray *sources;

- (id)init;

- (NSDictionary *)serialize;
- (void)deserialize:(NSDictionary *)dict;

@end
