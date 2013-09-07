#include "p1stream.h"

#include <AudioToolbox/AudioToolbox.h>


static const UInt32 num_buffers = 3;
static const UInt32 num_channels = 2;
static const UInt32 sample_size = sizeof(float);
static const UInt32 sample_size_bits = sample_size * 8;
static const UInt32 sample_rate = 44100;

typedef struct _P1InputAudioSource P1InputAudioSource;

struct _P1InputAudioSource {
    P1AudioSource super;

    AudioQueueRef queue;
    AudioQueueBufferRef buffers[num_buffers];
};

static void p1_input_audio_source_free(P1PluginElement *pel);
static bool p1_input_audio_source_start(P1PluginElement *pel);
static void p1_input_audio_source_stop(P1PluginElement *pel);
static void p1_input_audio_source_input_callback(
    void *inUserData,
    AudioQueueRef inAQ,
    AudioQueueBufferRef inBuffer,
    const AudioTimeStamp *inStartTime,
    UInt32 inNumberPacketDescriptions,
    const AudioStreamPacketDescription *inPacketDescs);


P1AudioSource *p1_input_audio_source_create(P1Config *cfg, P1ConfigSection *sect)
{
    P1InputAudioSource *iasrc = calloc(1, sizeof(P1InputAudioSource));
    P1AudioSource *asrc = (P1AudioSource *) iasrc;
    P1PluginElement *pel = (P1PluginElement *) iasrc;
    assert(iasrc != NULL);

    p1_audio_source_init(asrc, cfg, sect);

    pel->free = p1_input_audio_source_free;
    pel->start = p1_input_audio_source_start;
    pel->stop = p1_input_audio_source_stop;

    OSStatus ret;

    AudioStreamBasicDescription fmt;
    fmt.mFormatID = kAudioFormatLinearPCM;
    fmt.mFormatFlags = kLinearPCMFormatFlagIsFloat;
    fmt.mSampleRate = sample_rate;
    fmt.mBitsPerChannel = sample_size_bits;
    fmt.mChannelsPerFrame = num_channels;
    fmt.mBytesPerFrame = num_channels * sample_size;
    fmt.mFramesPerPacket = 1;
    fmt.mBytesPerPacket = fmt.mBytesPerFrame;
    fmt.mReserved = 0;

    ret = AudioQueueNewInput(&fmt, p1_input_audio_source_input_callback, asrc, NULL, kCFRunLoopCommonModes, 0, &iasrc->queue);
    assert(ret == noErr);

    char device[128];
    if (cfg->get_string(cfg, sect, "device", device, sizeof(device))) {
        CFStringRef str = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, device, kCFStringEncodingASCII, kCFAllocatorNull);
        if (str) {
            AudioQueueSetProperty(iasrc->queue, kAudioQueueProperty_CurrentDevice, &str, sizeof(str));
            CFRelease(str);
        }
    }

    for (int i = 0; i < num_buffers; i++) {
        ret = AudioQueueAllocateBuffer(iasrc->queue, 0x5000, &iasrc->buffers[i]);
        assert(ret == noErr);
        ret = AudioQueueEnqueueBuffer(iasrc->queue, iasrc->buffers[i], 0, NULL);
        assert(ret == noErr);
    }

    return asrc;
}

static void p1_input_audio_source_free(P1PluginElement *pel)
{
    P1InputAudioSource *iasrc = (P1InputAudioSource *) pel;

    AudioQueueDispose(iasrc->queue, TRUE);
}

static bool p1_input_audio_source_start(P1PluginElement *pel)
{
    P1Element *el = (P1Element *) pel;
    P1InputAudioSource *iasrc = (P1InputAudioSource *) pel;

    OSStatus ret = AudioQueueStart(iasrc->queue, NULL);
    assert(ret == noErr);

    // FIXME: Should we wait for anything?
    p1_set_state(el, P1_OTYPE_AUDIO_SOURCE, P1_STATE_RUNNING);

    return true;
}

static void p1_input_audio_source_stop(P1PluginElement *pel)
{
    P1Element *el = (P1Element *) pel;
    P1InputAudioSource *iasrc = (P1InputAudioSource *) pel;

    OSStatus ret = AudioQueueStop(iasrc->queue, TRUE);
    assert(ret == noErr);

    // FIXME: Async.
    p1_set_state(el, P1_OTYPE_AUDIO_SOURCE, P1_STATE_IDLE);
}

static void p1_input_audio_source_input_callback(
    void *inUserData,
    AudioQueueRef inAQ,
    AudioQueueBufferRef inBuffer,
    const AudioTimeStamp *inStartTime,
    UInt32 inNumberPacketDescriptions,
    const AudioStreamPacketDescription *inPacketDescs)
{
    P1AudioSource *asrc = (P1AudioSource *) inUserData;

    p1_audio_source_buffer(asrc, inStartTime->mHostTime, inBuffer->mAudioData,
                           inBuffer->mAudioDataByteSize / sample_size);

    OSStatus ret = AudioQueueEnqueueBuffer(inAQ, inBuffer, 0, NULL);
    assert(ret == noErr);
}
