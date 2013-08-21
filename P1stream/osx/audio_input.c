#include <stdio.h>
#include <dispatch/dispatch.h>
#include <AudioToolbox/AudioToolbox.h>

#include "p1stream.h"


static const UInt32 num_buffers = 3;
static const UInt32 num_channels = 2;
static const UInt32 sample_size = 2;
static const UInt32 sample_size_bits = sample_size * 8;
static const UInt32 sample_rate = 44100;

typedef struct _P1InputAudioSource P1InputAudioSource;

struct _P1InputAudioSource {
    P1AudioSource super;

    dispatch_queue_t dispatch;

    AudioQueueRef queue;
    AudioQueueBufferRef buffers[num_buffers];
};

static P1AudioSource *p1_input_audio_source_create();
static void p1_input_audio_source_free(P1AudioSource *_source);
static bool p1_input_audio_source_start(P1AudioSource *_source);
static void p1_input_audio_source_stop(P1AudioSource *_source);

P1AudioSourceFactory p1_input_audio_source_factory = {
    .create = p1_input_audio_source_create,
};


static P1AudioSource *p1_input_audio_source_create()
{
    P1InputAudioSource *source = calloc(1, sizeof(P1InputAudioSource));
    assert(source != NULL);

    P1AudioSource *_source = (P1AudioSource *) source;
    _source->factory = &p1_input_audio_source_factory;
    _source->free = p1_input_audio_source_free;
    _source->start = p1_input_audio_source_start;
    _source->stop = p1_input_audio_source_stop;

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

static void p1_input_audio_source_free(P1AudioSource *_source)
{
    P1InputAudioSource *source = (P1InputAudioSource *)_source;

    AudioQueueDispose(source->queue, TRUE);
    dispatch_release(source->dispatch);
}

static bool p1_input_audio_source_start(P1AudioSource *_source)
{
    P1InputAudioSource *source = (P1InputAudioSource *)_source;

    OSStatus ret = AudioQueueStart(source->queue, NULL);
    assert(ret == noErr);

    return true;
}

static void p1_input_audio_source_stop(P1AudioSource *_source)
{
    P1InputAudioSource *source = (P1InputAudioSource *)_source;

    OSStatus ret = AudioQueueStop(source->queue, TRUE);
    assert(ret == noErr);
}
