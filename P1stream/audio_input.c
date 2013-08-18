#include <stdio.h>
#include <dispatch/dispatch.h>
#include <AudioToolbox/AudioToolbox.h>

#include "audio.h"

static const UInt32 num_buffers = 3;
static const UInt32 num_channels = 2;
static const UInt32 sample_size = 2;
static const UInt32 sample_size_bits = sample_size * 8;
static const UInt32 sample_rate = 44100;

typedef struct _P1AudioInputSource P1AudioInputSource;

struct _P1AudioInputSource {
    P1AudioSource super;

    dispatch_queue_t dispatch;

    AudioQueueRef queue;
    AudioQueueBufferRef buffers[num_buffers];
};

static P1AudioSource *p1_audio_input_create();
static void p1_audio_input_free(P1AudioSource *_source);
static bool p1_audio_input_start(P1AudioSource *_source);
static void p1_audio_input_stop(P1AudioSource *_source);

P1AudioPlugin p1_audio_input = {
    .create = p1_audio_input_create,
    .free = p1_audio_input_free,

    .start = p1_audio_input_start,
    .stop = p1_audio_input_stop
};


static P1AudioSource *p1_audio_input_create()
{
    P1AudioInputSource *source = calloc(1, sizeof(P1AudioInputSource));
    assert(source != NULL);

    P1AudioSource *_source = (P1AudioSource *) source;
    _source->plugin = &p1_audio_input;

    OSStatus ret;

    source->dispatch = dispatch_queue_create("audio_input", DISPATCH_QUEUE_SERIAL);

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
        &source->queue, &fmt, 0, source->dispatch,
        ^(AudioQueueRef queue, AudioQueueBufferRef buf,
          const AudioTimeStamp *time, UInt32 num_descs,
          const AudioStreamPacketDescription *descs) {
            p1_audio_mix(_source, time->mHostTime, buf->mAudioData, buf->mAudioDataByteSize);

            OSStatus ret = AudioQueueEnqueueBuffer(source->queue, buf, 0, NULL);
            assert(ret == noErr);
        });
    assert(ret == noErr);

    for (int i = 0; i < num_buffers; i++) {
        ret = AudioQueueAllocateBuffer(source->queue, 0x5000, &source->buffers[i]);
        assert(ret == noErr);
        ret = AudioQueueEnqueueBuffer(source->queue, source->buffers[i], 0, NULL);
        assert(ret == noErr);
    }

    return _source;
}

static void p1_audio_input_free(P1AudioSource *_source)
{
    P1AudioInputSource *source = (P1AudioInputSource *)_source;

    AudioQueueDispose(source->queue, TRUE);
    dispatch_release(source->dispatch);
}

static bool p1_audio_input_start(P1AudioSource *_source)
{
    P1AudioInputSource *source = (P1AudioInputSource *)_source;

    OSStatus ret = AudioQueueStart(source->queue, NULL);
    assert(ret == noErr);

    return true;
}

static void p1_audio_input_stop(P1AudioSource *_source)
{
    P1AudioInputSource *source = (P1AudioInputSource *)_source;

    OSStatus ret = AudioQueueStop(source->queue, TRUE);
    assert(ret == noErr);
}
