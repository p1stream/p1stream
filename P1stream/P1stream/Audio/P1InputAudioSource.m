#import "P1InputAudioSource.h"


@implementation P1InputAudioSource

@synthesize delegate;

- (id)init
{
    self = [super init];
    if (self) {
        OSStatus err;

        bytesPerPacket = 2 * 32 / 8;
        const AudioStreamBasicDescription desc = {
            .mFormatID = kAudioFormatLinearPCM,
            .mFormatFlags = kLinearPCMFormatFlagIsFloat,
            .mSampleRate = 44100,
            .mChannelsPerFrame = 2,
            .mBitsPerChannel = 32,
            .mBytesPerFrame = bytesPerPacket,
            .mFramesPerPacket = 1,
            .mBytesPerPacket = bytesPerPacket
        };
        err = AudioQueueNewInput(&desc,
                                 audioQueueCallback, (__bridge void *)self,
                                 NULL, kCFRunLoopCommonModes,
                                 0, &audioQueue);
        if (err) {
            NSLog(@"Failed to create audio queue. (%d)", err);
            return nil;
        }

        for (size_t i = 0; i < P1_INPUT_AUDIO_SOURCE_NUM_BUFFERS; i++) {
            err = AudioQueueAllocateBuffer(audioQueue, P1_AUDIO_MIXER_BUFFER_SIZE, &audioBuffers[i]);
            if (err) {
                NSLog(@"Failed to allocate audio buffer. (%d)", err);
                return nil;
            }

            // Abuse user data to signal 'ready'.
            audioBuffers[i]->mUserData = 0;
        }
    }
    return self;
}

- (void)dealloc
{
    if (audioQueue) {
        OSStatus err = AudioQueueDispose(audioQueue, true);
        if (err) {
            NSLog(@"Failed to dispose of audio queue. (%d)", err);
        }
    }
}

- (P1AudioSourceSlot *)slot
{
    @synchronized(self) {
        return slot;
    }
}

- (void)setSlot:(P1AudioSourceSlot *)slot_
{
    @synchronized(self) {
        if (slot_ == slot) return;

        [self cleanupState];

        slot = slot_;

        [self setupState];
    }
}

- (NSString *)deviceUID
{
    @synchronized(self) {
        return deviceUID;
    }
}

- (void)setDeviceUID:(NSString *)deviceUID_
{
    @synchronized(self) {
        if (deviceUID_ == deviceUID) return;

        [self cleanupState];

        deviceUID = deviceUID_;

        [self setupState];
    }
}

// Private. Set up state linking parameters together.
- (void)setupState
{
    // Check if all properties are set.
    if (!slot) return;
    if (!deviceUID) return;

    OSStatus err;

    const CFStringRef cfDeviceUID = (__bridge CFStringRef)deviceUID;
    err = AudioQueueSetProperty(audioQueue, kAudioQueueProperty_CurrentDevice,
                                &cfDeviceUID, sizeof(CFStringRef));
    if (err) {
        NSLog(@"Failed to set input device on audio queue. (%d)", err);
        return;
    }

    err = AudioQueueStart(audioQueue, NULL);
    if (err) {
        NSLog(@"Failed to start audio queue. (%d)", err);
        return;
    }
    running = TRUE;

    for (size_t i = 0; i < P1_INPUT_AUDIO_SOURCE_NUM_BUFFERS; i++) {
        err = AudioQueueEnqueueBuffer(audioQueue, audioBuffers[i], 0, NULL);
        if (err) {
            NSLog(@"Failed to enqueue audio buffer during setup. (%d)", err);
            [self cleanupState];
            return;
        }
    }
}

// Private. Clean up above state. Also called from dealloc.
- (void)cleanupState
{
    if (running) {
        running = FALSE;
        OSStatus err = AudioQueueStop(audioQueue, true);
        if (err) {
            NSLog(@"Failed to stop audio queue. (%d)", err);
        }
    }

    for (size_t i = 0; i < P1_INPUT_AUDIO_SOURCE_NUM_BUFFERS; i++)
        audioBuffers[i]->mUserData = 0;
    currentBuffer = NULL;
}

// Private. Capture audio from a single buffer.
- (void)captureBuffer:(AudioQueueBufferRef)buffer
{
    id delegate_;

    @synchronized (self) {
        if (!running) return;

        buffer->mUserData = 1;
        if (!currentBuffer)
            currentBuffer = buffer;

        delegate_ = delegate;
    }

    if (delegate_) {
        dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
        dispatch_async(queue, ^{
            [delegate_ audioSourceClockTick];
        });
    }
}

// Audio queue input callback. Simply wraps `captureBuffer`.
static void audioQueueCallback(void *userData,
                               AudioQueueRef audioQueue,
                               AudioQueueBufferRef buffer,
                               const AudioTimeStamp *startTime,
                               UInt32 numPackets,
                               const AudioStreamPacketDescription *descs)
{
    @autoreleasepool {
        P1InputAudioSource *self = (__bridge P1InputAudioSource *)userData;
        [self captureBuffer:buffer];
    }
}

- (Float32 *)getCurrentBufferLocked
{
    @synchronized (self) {
        if (!currentBuffer)
            return NULL;

        Float32 *result = currentBuffer->mAudioData;
        currentBuffer->mUserData = 0;

        size_t i;
        for (i = 0; i < P1_INPUT_AUDIO_SOURCE_NUM_BUFFERS; i++) {
            if (audioBuffers[i] == currentBuffer)
                break;
        }
        size_t end = i;
        do {
            i = (i + 1) % P1_INPUT_AUDIO_SOURCE_NUM_BUFFERS;
            currentBuffer = audioBuffers[i];
            if (currentBuffer->mUserData == (void *)1)
                break;
            else
                currentBuffer = NULL;
        } while (i != end);

        return result;
    }
}

- (void)unlockBuffer:(Float32 *)buffer
{
    @synchronized (self) {
        AudioQueueBufferRef match = NULL;
        for (size_t i = 0; i < P1_INPUT_AUDIO_SOURCE_NUM_BUFFERS; i++) {
            match = audioBuffers[i];
            if (match->mAudioData == buffer)
                break;
            else
                match = NULL;
        }
        if (!match) {
            NSLog(@"Tried to unlock buffer that doesn't exist. (%p)", buffer);
            return;
        }

        OSStatus err = AudioQueueEnqueueBuffer(audioQueue, match, 0, NULL);
        if (err)
            NSLog(@"Failed to enqueue audio buffer after processing. (%d)", err);
    }
}

- (NSDictionary *)serialize
{
    return @{
        @"deviceUID" : deviceUID
    };
}

- (void)deserialize:(NSDictionary *)dict
{
    self.deviceUID = [dict objectForKey:@"deviceUID"];
}

@end
