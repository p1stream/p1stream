#import "P1AudioMixer.h"


@interface P1HALAudioSource : NSObject <P1AudioSource>

@property (retain) P1AudioSourceSlot *slot;

@end
