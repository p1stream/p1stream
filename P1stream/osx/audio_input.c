#include "p1stream.h"

#include <stdio.h>
#include <dispatch/dispatch.h>
#include <AudioToolbox/AudioToolbox.h>


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

static void p1_input_audio_source_free(P1Source *src);
static bool p1_input_audio_source_start(P1Source *src);
static void p1_input_audio_source_stop(P1Source *src);


P1AudioSource *p1_input_audio_source_create()
{
    P1InputAudioSource *iasrc = calloc(1, sizeof(P1InputAudioSource));
    P1AudioSource *asrc = (P1AudioSource *) iasrc;
    P1Source *src = (P1Source *) iasrc;
    assert(iasrc != NULL);

    src->free = p1_input_audio_source_free;
    src->start = p1_input_audio_source_start;
    src->stop = p1_input_audio_source_stop;

    OSStatus ret;

    iasrc->dispatch = dispatch_queue_create("audio_input", DISPATCH_QUEUE_SERIAL);

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
        &iasrc->queue, &fmt, 0, iasrc->dispatch,
        ^(AudioQueueRef queue, AudioQueueBufferRef buf,
          const AudioTimeStamp *time, UInt32 num_descs,
          const AudioStreamPacketDescription *descs) {
            p1_audio_buffer(asrc, time->mHostTime, buf->mAudioData, buf->mAudioDataByteSize);

            OSStatus ret = AudioQueueEnqueueBuffer(iasrc->queue, buf, 0, NULL);
            assert(ret == noErr);
        });
    assert(ret == noErr);

    for (int i = 0; i < num_buffers; i++) {
        ret = AudioQueueAllocateBuffer(iasrc->queue, 0x5000, &iasrc->buffers[i]);
        assert(ret == noErr);
        ret = AudioQueueEnqueueBuffer(iasrc->queue, iasrc->buffers[i], 0, NULL);
        assert(ret == noErr);
    }

    return asrc;
}

static void p1_input_audio_source_free(P1Source *src)
{
    P1InputAudioSource *iasrc = (P1InputAudioSource *)src;

    AudioQueueDispose(iasrc->queue, TRUE);
    dispatch_release(iasrc->dispatch);
}

static bool p1_input_audio_source_start(P1Source *src)
{
    P1InputAudioSource *iasrc = (P1InputAudioSource *)src;

    OSStatus ret = AudioQueueStart(iasrc->queue, NULL);
    assert(ret == noErr);

    // FIXME: Should we wait for anything?
    p1_set_state(src->ctx, P1_OTYPE_AUDIO_SOURCE, src, P1_STATE_RUNNING);

    return true;
}

static void p1_input_audio_source_stop(P1Source *src)
{
    P1InputAudioSource *iasrc = (P1InputAudioSource *)src;

    OSStatus ret = AudioQueueStop(iasrc->queue, TRUE);
    assert(ret == noErr);

    // FIXME: Async.
    p1_set_state(src->ctx, P1_OTYPE_AUDIO_SOURCE, src, P1_STATE_IDLE);
}
