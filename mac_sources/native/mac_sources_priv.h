#ifndef p1_mac_sources_priv_h
#define p1_mac_sources_priv_h

#include "core.h"

#include <CoreVideo/CoreVideo.h>

namespace p1stream {


extern Persistent<String> display_id_sym;
extern Persistent<String> divisor_sym;


class display_link : public video_clock {
private:
    CGDirectDisplayID display_id;
    uint32_t divisor;

    CVDisplayLinkRef cv_handle;
    bool running;
    int skip_counter;

    video_mixer *mixer;

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

    // Video clock implementation.
    virtual fraction_t ticks_per_second() final;
    virtual void ref_mixer(video_mixer *mixer) final;
    virtual void unref_mixer(video_mixer *mixer) final;

    // Module init.
    static void init_prototype(Handle<FunctionTemplate> func);
};


class display_stream : public video_source, public lockable {
private:
    CGDirectDisplayID display_id;

    dispatch_queue_t dispatch;
    CGDisplayStreamRef cg_handle;
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
    virtual void frame(video_mixer *mixer) final;
    virtual void ref_mixer(video_mixer *mixer) final;
    virtual void unref_mixer(video_mixer *mixer) final;

    // Module init.
    static void init_prototype(Handle<FunctionTemplate> func);
};


}  // namespace p1stream

#endif  // p1_mac_sources_priv_h
