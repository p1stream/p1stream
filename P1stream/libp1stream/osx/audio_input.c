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

static bool p1_input_audio_source_init(P1InputAudioSource *iasrc);
static void p1_input_audio_source_config(P1Plugin *pel, P1Config *cfg);
static void p1_input_audio_source_start(P1Plugin *pel);
static void p1_input_audio_source_stop(P1Plugin *pel);
static void p1_input_audio_source_kill_session(P1InputAudioSource *iasrc);
static void p1_input_audio_source_halt(P1InputAudioSource *iasrc);
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


P1AudioSource *p1_input_audio_source_create()
{
    P1InputAudioSource *iasrc = calloc(1, sizeof(P1InputAudioSource));

    if (iasrc != NULL) {
        if (!p1_input_audio_source_init(iasrc)) {
            free(iasrc);
            iasrc = NULL;
        }
    }

    return (P1AudioSource *) iasrc;
}

static bool p1_input_audio_source_init(P1InputAudioSource *iasrc)
{
    P1AudioSource *asrc = (P1AudioSource *) iasrc;
    P1Plugin *pel = (P1Plugin *) iasrc;

    if (!p1_audio_source_init(asrc))
        return false;

    pel->config = p1_input_audio_source_config;
    pel->start = p1_input_audio_source_start;
    pel->stop = p1_input_audio_source_stop;

    return true;
}

static void p1_input_audio_source_config(P1Plugin *pel, P1Config *cfg)
{
    P1InputAudioSource *iasrc = (P1InputAudioSource *) pel;

    cfg->get_string(cfg, "device", iasrc->device, sizeof(iasrc->device));
}

static void p1_input_audio_source_start(P1Plugin *pel)
{
    P1Object *obj = (P1Object *) pel;
    P1AudioSource *asrc = (P1AudioSource *) pel;
    P1InputAudioSource *iasrc = (P1InputAudioSource *) pel;
    OSStatus ret;

    p1_object_set_state(obj, P1_STATE_STARTING);

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
         goto fail;

    ret = AudioQueueAddPropertyListener(iasrc->queue, kAudioQueueProperty_IsRunning, p1_input_audio_source_running_callback, iasrc);
    if (ret != noErr)
         goto fail;

    if (iasrc->device[0]) {
        CFStringRef str = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, iasrc->device, kCFStringEncodingUTF8, kCFAllocatorNull);
        if (str) {
            ret = AudioQueueSetProperty(iasrc->queue, kAudioQueueProperty_CurrentDevice, &str, sizeof(str));
            CFRelease(str);
            if (ret != noErr)
                 goto fail;
        }
    }

    for (int i = 0; i < num_buffers; i++) {
        ret = AudioQueueAllocateBuffer(iasrc->queue, 0x5000, &iasrc->buffers[i]);
        if (ret != noErr)
            goto fail;

        ret = AudioQueueEnqueueBuffer(iasrc->queue, iasrc->buffers[i], 0, NULL);
        if (ret != noErr) {
            AudioQueueFreeBuffer(iasrc->queue, iasrc->buffers[i]);
            goto fail;
        }
    }

    // Async, waits until running callback.
    ret = AudioQueueStart(iasrc->queue, NULL);
    if (ret != noErr)
        goto fail;

    return;

fail:
    p1_log(obj, P1_LOG_ERROR, "Failed to setup audio queue");
    p1_log_os_status(obj, P1_LOG_ERROR, ret);
    p1_input_audio_source_halt(iasrc);
}

static void p1_input_audio_source_stop(P1Plugin *pel)
{
    P1Object *obj = (P1Object *) pel;
    P1InputAudioSource *iasrc = (P1InputAudioSource *) pel;
    OSStatus ret;

    p1_object_set_state(obj, P1_STATE_STOPPING);

    // Async, waits until running callback.
    ret = AudioQueueStop(iasrc->queue, FALSE);
    if (ret != noErr) {
        p1_log(obj, P1_LOG_ERROR, "Failed to stop audio queue");
        p1_log_os_status(obj, P1_LOG_ERROR, ret);
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
            p1_log(obj, P1_LOG_ERROR, "Failed to dispose of audio queue");
            p1_log_os_status(obj, P1_LOG_ERROR, ret);
            p1_input_audio_source_halt(iasrc);
        }
    }
}

static void p1_input_audio_source_halt(P1InputAudioSource *iasrc)
{
    P1Object *obj = (P1Object *) iasrc;

    p1_object_set_state(obj, P1_STATE_HALTING);
    p1_input_audio_source_kill_session(iasrc);
    p1_object_set_state(obj, P1_STATE_HALTED);
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
        p1_log(obj, P1_LOG_ERROR, "Failed to return buffer to audio queue");
        p1_log_os_status(obj, P1_LOG_ERROR, ret);

        p1_object_lock(obj);
        p1_input_audio_source_halt(iasrc);
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
        p1_log(obj, P1_LOG_ERROR, "Failed to get audio queue status");
        p1_log_os_status(obj, P1_LOG_ERROR, ret);
        p1_input_audio_source_halt(iasrc);
        goto end;
    }

    if (running) {
        // Confirm start.
        if (obj->state == P1_STATE_STARTING)
            p1_object_set_state(obj, P1_STATE_RUNNING);
    }
    else {
        // Clean up after stopping.
        p1_input_audio_source_kill_session(iasrc);

        // Check if this was a proper shutdown.
        if (obj->state == P1_STATE_STOPPING) {
            p1_object_set_state(obj, P1_STATE_IDLE);
        }
        else if (obj->state == P1_STATE_RUNNING) {
            p1_log(obj, P1_LOG_ERROR, "Audio queue stopped itself");
            p1_input_audio_source_halt(iasrc);
        }
    }

end:
    p1_object_unlock(obj);
}
