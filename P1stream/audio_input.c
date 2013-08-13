#include <stdio.h>
#include <dispatch/dispatch.h>
#include <AudioToolbox/AudioToolbox.h>

#include "audio_input.h"

#include "stream.h"

static const UInt32 num_buffers = 3;
static const UInt32 num_channels = 2;
static const UInt32 sample_size_bits = 16;
static const UInt32 sample_rate = 44100;

struct {
    dispatch_queue_t dispatch;

    AudioQueueRef queue;
    AudioQueueBufferRef buffers[num_buffers];
    int sent_config;
} state;

static void p1_audio_input_callback(
    AudioQueueBufferRef buf, const AudioTimeStamp *time,
    UInt32 num_packets, const AudioStreamPacketDescription *packets);


void p1_audio_input_init()
{
    OSStatus ret;

    state.dispatch = dispatch_queue_create("audio_input", DISPATCH_QUEUE_SERIAL);

    // FIXME: don't do encoding here, but after mixing
    AudioStreamBasicDescription fmt;
    memset(&fmt, 0, sizeof(AudioStreamBasicDescription));
    fmt.mSampleRate = sample_rate;
    fmt.mFormatID = kAudioFormatMPEG4AAC;
    fmt.mChannelsPerFrame = num_channels;
    fmt.mFramesPerPacket = 1024;

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
    if (!state.sent_config) {
        state.sent_config = 1;
        p1_stream_audio_config();
    }

    p1_stream_audio(buf, time->mHostTime);

    OSStatus ret = AudioQueueEnqueueBuffer(state.queue, buf, 0, NULL);
    assert(ret == noErr);
}
