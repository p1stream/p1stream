#include "mac_sources_priv.h"

namespace p1_mac_sources {


static const UInt32 num_channels = 2;
static const UInt32 sample_size = sizeof(float);
static const UInt32 sample_size_bits = sample_size * 8;
static const UInt32 sample_rate = 44100;


Handle<Value> audio_queue::init(const Arguments &args)
{
    bool ok = true;
    Handle<Value> ret;
    char err[128];

    OSStatus os_ret;

    Handle<Object> params;
    Handle<Value> val;

    Wrap(args.This());

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

    if (args.Length() == 1) {
        if (!(ok = args[0]->IsObject()))
            ret = Exception::TypeError(
                String::New("Expected an object"));
        else
            params = Local<Object>::Cast(args[0]);
    }

    if (ok) {
        os_ret = AudioQueueNewInput(&fmt, input_callback, this, NULL, kCFRunLoopCommonModes, 0, &queue);
        if (!(ok = (os_ret == noErr)))
            sprintf(err, "AudioQueueNewInput error %d", os_ret);
    }

    if (ok && !params.IsEmpty()) {
        val = params->Get(device_sym);
        if (!val->IsUndefined()) {
            String::Utf8Value strVal(val);
            if (!(ok = (*strVal != NULL))) {
                ret = Exception::TypeError(
                    String::New("Invalid device value"));
            }
            else {
                CFStringRef str = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, *strVal, kCFStringEncodingUTF8, kCFAllocatorNull);
                if (!str)
                    abort();

                os_ret = AudioQueueSetProperty(queue, kAudioQueueProperty_CurrentDevice, &str, sizeof(str));
                CFRelease(str);
                if (!(ok = (os_ret == noErr)))
                    sprintf(err, "AudioQueueSetProperty error %d", os_ret);
            }
        }
    }

    if (ok) {
        for (int i = 0; i < num_buffers; i++) {
            os_ret = AudioQueueAllocateBuffer(queue, 0x5000, &buffers[i]);
            if (!(ok = (os_ret == noErr))) {
                sprintf(err, "AudioQueueAllocateBuffer error %d", os_ret);
                break;
            }

            os_ret = AudioQueueEnqueueBuffer(queue, buffers[i], 0, NULL);
            if (!(ok = (os_ret == noErr))) {
                sprintf(err, "AudioQueueEnqueueBuffer error %d", os_ret);
                AudioQueueFreeBuffer(queue, buffers[i]);
                break;
            }
        }
    }

    if (ok) {
        // Async, waits until running callback.
        os_ret = AudioQueueStart(queue, NULL);
        if (!(ok = (os_ret == noErr)))
            sprintf(err, "AudioQueueStart error %d", os_ret);
    }

    if (ok) {
        Ref();
        return handle_;
    }
    else {
        destroy(false);

        if (ret.IsEmpty())
            ret = Exception::Error(String::New(err));
        return ThrowException(ret);
    }
}

void audio_queue::destroy(bool unref)
{
    if (queue != NULL) {
        OSStatus ret = AudioQueueDispose(queue, FALSE);
        queue = NULL;
        if (ret != noErr)
            fprintf(stderr, "AudioQueueDispose error %d\n", ret);
    }

    if (unref)
        Unref();
}

void audio_queue::link_audio_source(audio_source_context &ctx_)
{
    if (ctx == nullptr) {
        ctx = &ctx_;
    }
    else {
        // FIXME: error
    }
}

void audio_queue::unlink_audio_source(audio_source_context &ctx_)
{
    if (ctx == &ctx_) {
        ctx = nullptr;
    }
    else {
        // FIXME: error
    }
}

void audio_queue::input_callback(
    void *inUserData,
    AudioQueueRef inAQ,
    AudioQueueBufferRef inBuffer,
    const AudioTimeStamp *inStartTime,
    UInt32 inNumberPacketDescriptions,
    const AudioStreamPacketDescription *inPacketDescs)
{
    auto *ctx = ((audio_queue *) inUserData)->ctx;
    if (ctx)
        ctx->render_buffer(inStartTime->mHostTime, (float *) inBuffer->mAudioData,
                           inBuffer->mAudioDataByteSize / sample_size);

    OSStatus ret = AudioQueueEnqueueBuffer(inAQ, inBuffer, 0, NULL);
    if (ret != noErr)
        fprintf(stderr, "AudioQueueEnqueueBuffer error %d\n", ret);
}

void audio_queue::init_prototype(Handle<FunctionTemplate> func)
{
    SetPrototypeMethod(func, "destroy", [](const Arguments &args) -> Handle<Value> {
        auto link = ObjectWrap::Unwrap<audio_queue>(args.This());
        link->destroy();
        return Undefined();
    });
}


}  // namespace p1_mac_sources
