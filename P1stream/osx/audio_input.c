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

    char device[128];

    AudioQueueRef queue;
    AudioQueueBufferRef buffers[num_buffers];
};

static void p1_input_audio_source_start(P1Plugin *pel);
static void p1_input_audio_source_stop(P1Plugin *pel);
static void p1_input_audio_source_kill_session(P1InputAudioSource *iasrc);
static void p1_input_audio_source_input_callback(
    void *inUserData,
    AudioQueueRef inAQ,
    AudioQueueBufferRef inBuffer,
    const AudioTimeStamp *inStartTime,
    UInt32 inNumberPacketDescriptions,
    const AudioStreamPacketDescription *inPacketDescs);
static void p1_input_audio_source_running_callback(
    void *inUserData,
    AudioQueueRef inAQ,
    AudioQueuePropertyID inID);


P1AudioSource *p1_input_audio_source_create(P1Config *cfg, P1ConfigSection *sect)
{
    P1InputAudioSource *iasrc = calloc(1, sizeof(P1InputAudioSource));
    P1AudioSource *asrc = (P1AudioSource *) iasrc;
    P1Plugin *pel = (P1Plugin *) iasrc;

    if (iasrc != NULL) {
        p1_audio_source_init(asrc, cfg, sect);

        pel->start = p1_input_audio_source_start;
        pel->stop = p1_input_audio_source_stop;

        cfg->get_string(cfg, sect, "device", iasrc->device, sizeof(iasrc->device));
    }

    return asrc;
}

static void p1_input_audio_source_start(P1Plugin *pel)
{
    P1Object *obj = (P1Object *) pel;
    P1AudioSource *asrc = (P1AudioSource *) pel;
    P1InputAudioSource *iasrc = (P1InputAudioSource *) pel;
    OSStatus ret;

    p1_object_set_state(obj, P1_OTYPE_AUDIO_SOURCE, P1_STATE_STARTING);

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
    if (ret != noErr)
         goto halt;

    ret = AudioQueueAddPropertyListener(iasrc->queue, kAudioQueueProperty_IsRunning, p1_input_audio_source_running_callback, iasrc);
    if (ret != noErr)
         goto halt;

    if (iasrc->device[0]) {
        CFStringRef str = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, iasrc->device, kCFStringEncodingUTF8, kCFAllocatorNull);
        if (str) {
            ret = AudioQueueSetProperty(iasrc->queue, kAudioQueueProperty_CurrentDevice, &str, sizeof(str));
            CFRelease(str);
            if (ret != noErr)
                 goto halt;
        }
    }

    for (int i = 0; i < num_buffers; i++) {
        ret = AudioQueueAllocateBuffer(iasrc->queue, 0x5000, &iasrc->buffers[i]);
        if (ret != noErr)
            goto halt;

        ret = AudioQueueEnqueueBuffer(iasrc->queue, iasrc->buffers[i], 0, NULL);
        if (ret != noErr) {
            AudioQueueFreeBuffer(iasrc->queue, iasrc->buffers[i]);
            goto halt;
        }
    }

    ret = AudioQueueStart(iasrc->queue, NULL);
    if (ret != noErr)
        goto halt;

    return;

halt:
    p1_log(obj, P1_LOG_ERROR, "Failed to setup audio queue\n");
    p1_log_os_status(obj, P1_LOG_ERROR, ret);
    p1_input_audio_source_kill_session(iasrc);
    p1_object_set_state(obj, P1_OTYPE_AUDIO_SOURCE, P1_STATE_HALTED);
}

static void p1_input_audio_source_stop(P1Plugin *pel)
{
    P1Object *obj = (P1Object *) pel;
    P1InputAudioSource *iasrc = (P1InputAudioSource *) pel;
    OSStatus ret;

    p1_object_set_state(obj, P1_OTYPE_AUDIO_SOURCE, P1_STATE_STOPPING);

    ret = AudioQueueStop(iasrc->queue, FALSE);
    if (ret != noErr) {
        p1_log(obj, P1_LOG_ERROR, "Failed to stop audio queue\n");
        p1_log_os_status(obj, P1_LOG_ERROR, ret);
        p1_input_audio_source_kill_session(iasrc);
        p1_object_set_state(obj, P1_OTYPE_AUDIO_SOURCE, P1_STATE_HALTED);
    }
}

static void p1_input_audio_source_kill_session(P1InputAudioSource *iasrc)
{
    P1Object *obj = (P1Object *) iasrc;
    OSStatus ret;

    if (iasrc->queue) {
        ret = AudioQueueDispose(iasrc->queue, TRUE);
        iasrc->queue = NULL;
        if (ret != noErr) {
            p1_log(obj, P1_LOG_ERROR, "Failed to dispose of audio queue\n");
            p1_log_os_status(obj, P1_LOG_ERROR, ret);
        }
    }
}

static void p1_input_audio_source_input_callback(
    void *inUserData,
    AudioQueueRef inAQ,
    AudioQueueBufferRef inBuffer,
    const AudioTimeStamp *inStartTime,
    UInt32 inNumberPacketDescriptions,
    const AudioStreamPacketDescription *inPacketDescs)
{
    P1Object *obj = (P1Object *) inUserData;
    P1AudioSource *asrc = (P1AudioSource *) inUserData;
    P1InputAudioSource *iasrc = (P1InputAudioSource *) inUserData;

    // FIXME: should we worry about this being atomic?
    if (obj->state == P1_STATE_RUNNING)
        p1_audio_source_buffer(asrc, inStartTime->mHostTime, inBuffer->mAudioData,
                               inBuffer->mAudioDataByteSize / sample_size);

    OSStatus ret = AudioQueueEnqueueBuffer(inAQ, inBuffer, 0, NULL);
    if (ret != noErr) {
        p1_log(obj, P1_LOG_ERROR, "Failed to return buffer to audio queue\n");
        p1_log_os_status(obj, P1_LOG_ERROR, ret);

        p1_object_lock(obj);

        p1_input_audio_source_kill_session(iasrc);
        p1_object_set_state(obj, P1_OTYPE_AUDIO_SOURCE, P1_STATE_HALTED);

        p1_object_unlock(obj);
    }
}

static void p1_input_audio_source_running_callback(
    void *inUserData,
    AudioQueueRef inAQ,
    AudioQueuePropertyID inID)
{
    P1InputAudioSource *iasrc = (P1InputAudioSource *) inUserData;
    P1Object *obj = (P1Object *) inUserData;
    OSStatus ret;
    UInt32 running;
    UInt32 size;

    p1_object_lock(obj);

    size = sizeof(running);
    ret = AudioQueueGetProperty(inAQ, kAudioQueueProperty_IsRunning, &running, &size);
    if (ret != noErr) {
        p1_log(obj, P1_LOG_ERROR, "Failed to get audio queue status\n");
        p1_log_os_status(obj, P1_LOG_ERROR, ret);
        p1_input_audio_source_kill_session(iasrc);
        p1_object_set_state(obj, P1_OTYPE_AUDIO_SOURCE, P1_STATE_HALTED);
        goto end;
    }

    if (running) {
        // Confirm start.
        if (obj->state == P1_STATE_STARTING)
            p1_object_set_state(obj, P1_OTYPE_AUDIO_SOURCE, P1_STATE_RUNNING);
    }
    else {
        // Clean up after stopping.
        p1_input_audio_source_kill_session(iasrc);

        // Check if this was a proper shutdown.
        if (obj->state == P1_STATE_STOPPING) {
            p1_object_set_state(obj, P1_OTYPE_AUDIO_SOURCE, P1_STATE_IDLE);
        }
        else if (obj->state == P1_STATE_RUNNING) {
            p1_log(obj, P1_LOG_ERROR, "Audio queue stopped itself\n");
            p1_object_set_state(obj, P1_OTYPE_AUDIO_SOURCE, P1_STATE_HALTED);
        }
    }

end:
    p1_object_unlock(obj);
}
