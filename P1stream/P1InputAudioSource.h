#import "P1AudioMixer.h"
#include <CoreAudio/CoreAudio.h>
#include <AudioToolbox/AudioToolbox.h>

#define P1_INPUT_AUDIO_SOURCE_NUM_BUFFERS 3


@interface P1InputAudioSource : NSObject <P1AudioSource>
{
    AudioQueueRef audioQueue;
    UInt32 bytesPerPacket;
    AudioQueueBufferRef audioBuffers[P1_INPUT_AUDIO_SOURCE_NUM_BUFFERS];
    AudioQueueBufferRef currentBuffer;
    BOOL running;

    P1AudioSourceSlot *slot;
    NSString *deviceUID;
}

@property (retain) id delegate;
@property (retain) P1AudioSourceSlot *slot;
@property (retain) NSString *deviceUID;

@end
