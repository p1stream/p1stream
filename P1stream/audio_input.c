#include <stdio.h>
#include <dispatch/dispatch.h>
#include <AudioToolbox/AudioToolbox.h>

#include "audio_input.h"
#include "audio.h"

static const UInt32 num_buffers = 3;
static const UInt32 num_channels = 2;
static const UInt32 sample_size = 2;
static const UInt32 sample_size_bits = sample_size * 8;
static const UInt32 sample_rate = 44100;

static struct {
    dispatch_queue_t dispatch;

    AudioQueueRef queue;
    AudioQueueBufferRef buffers[num_buffers];
} state;

static void p1_audio_input_callback(
    AudioQueueBufferRef buf, const AudioTimeStamp *time,
    UInt32 num_packets, const AudioStreamPacketDescription *packets);


void p1_audio_input_init()
{
    OSStatus ret;

    state.dispatch = dispatch_queue_create("audio_input", DISPATCH_QUEUE_SERIAL);

    AudioStreamBasicDescription fmt;
    fmt.mFormatID = kAudioFormatLinearPCM;
    fmt.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger;
    fmt.mSampleRate = sample_rate;
    fmt.mBitsPerChannel = sample_size_bits;
    fmt.mChannelsPerFrame = num_channels;
    fmt.mBytesPerFrame = num_channels * sample_size;
    fmt.mFramesPerPacket = 1;
    fmt.mBytesPerPacket = fmt.mBytesPerFrame;
    fmt.mReserved = 0;

    ret = AudioQueueNewInputWithDispatchQueue(
        &state.queue, &fmt, 0, state.dispatch,
        ^(AudioQueueRef queue, AudioQueueBufferRef buf,
          const AudioTimeStamp *time, UInt32 num_descs,
          const AudioStreamPacketDescription *descs) {
            p1_audio_input_callback(buf, time, num_descs, descs);
        });
    assert(ret == noErr);

    for (int i = 0; i < num_buffers; i++) {
        ret = AudioQueueAllocateBuffer(state.queue, 0x5000, &state.buffers[i]);
        assert(ret == noErr);
        ret = AudioQueueEnqueueBuffer(state.queue, state.buffers[i], 0, NULL);
        assert(ret == noErr);
    }

    ret = AudioQueueStart(state.queue, NULL);
    assert(ret == noErr);
}

static void p1_audio_input_callback(
    AudioQueueBufferRef buf, const AudioTimeStamp *time,
    UInt32 num_packets, const AudioStreamPacketDescription *packets)
{
    p1_audio_mix(time->mHostTime, buf->mAudioData, buf->mAudioDataByteSize);

    OSStatus ret = AudioQueueEnqueueBuffer(state.queue, buf, 0, NULL);
    assert(ret == noErr);
}
