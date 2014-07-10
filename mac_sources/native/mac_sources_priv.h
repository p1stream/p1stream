#ifndef p1_mac_sources_priv_h
#define p1_mac_sources_priv_h

#include "core.h"

#include <CoreVideo/CoreVideo.h>
#include <AudioToolbox/AudioToolbox.h>

namespace p1stream {


extern Persistent<String> display_id_sym;
extern Persistent<String> divisor_sym;
extern Persistent<String> device_sym;


class display_link : public video_clock {
private:
    uint32_t divisor;

    CVDisplayLinkRef cv_handle;
    lockable_mutex mutex;
    bool running;
    int skip_counter;

    video_clock_context *ctx;

    static CVReturn callback(
        CVDisplayLinkRef display_link,
        const CVTimeStamp *now,
        const CVTimeStamp *output_time,
        CVOptionFlags flags_in,
        CVOptionFlags *flags_out,
        void *context);
    void tick(frame_time_t time);

public:
    // Public JavaScript methods.
    Handle<Value> init(const Arguments &args);
    void destroy(bool unref = true);

    // Lockable implementation.
    virtual lockable *lock() final;

    // Video clock implementation.
    virtual void link_video_clock(video_clock_context &ctx_) final;
    virtual void unlink_video_clock(video_clock_context &ctx_) final;
    virtual fraction_t video_ticks_per_second(video_clock_context &ctx) final;

    // Module init.
    static void init_prototype(Handle<FunctionTemplate> func);
};


class display_stream : public video_source {
private:
    dispatch_queue_t dispatch;
    CGDisplayStreamRef cg_handle;
    lockable_mutex mutex;
    bool running;

    IOSurfaceRef last_frame;

    void callback(
        CGDisplayStreamFrameStatus status,
        IOSurfaceRef frame);

public:
    // Public JavaScript methods.
    Handle<Value> init(const Arguments &args);
    void destroy(bool unref = true);

    // Video source implementation.
    virtual void produce_video_frame(video_source_context &ctx) final;

    // Module init.
    static void init_prototype(Handle<FunctionTemplate> func);
};


class audio_queue : public audio_source {
private:
    static const UInt32 num_buffers = 3;

    AudioQueueRef queue;
    AudioQueueBufferRef buffers[num_buffers];

    audio_source_context *ctx;

    static void input_callback(
        void *inUserData,
        AudioQueueRef inAQ,
        AudioQueueBufferRef inBuffer,
        const AudioTimeStamp *inStartTime,
        UInt32 inNumberPacketDescriptions,
        const AudioStreamPacketDescription *inPacketDescs);

public:
    // Public JavaScript methods.
    Handle<Value> init(const Arguments &args);
    void destroy(bool unref = true);

    // Audio source implementation.
    virtual void link_audio_source(audio_source_context &ctx) final;
    virtual void unlink_audio_source(audio_source_context &ctx) final;

    // Module init.
    static void init_prototype(Handle<FunctionTemplate> func);
};


}  // namespace p1stream

#endif  // p1_mac_sources_priv_h
