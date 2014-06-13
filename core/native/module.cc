#include "core_priv.h"

#include <mach/mach_time.h>
#include <mach/mach_error.h>

#if __APPLE__
#   include <TargetConditionals.h>
#   if TARGET_OS_MAC
#       include "core_priv_mac.h"
#       define video_mixer_platform video_mixer_mac
#   endif
#endif
#ifndef video_mixer_platform
#   error Platform not supported
#endif

namespace p1stream {


Persistent<String> buffer_size_sym;
Persistent<String> width_sym;
Persistent<String> height_sym;
Persistent<String> x264_preset_sym;
Persistent<String> x264_tuning_sym;
Persistent<String> x264_params_sym;
Persistent<String> x264_profile_sym;
Persistent<String> source_sym;
Persistent<String> x1_sym;
Persistent<String> y1_sym;
Persistent<String> x2_sym;
Persistent<String> y2_sym;
Persistent<String> u1_sym;
Persistent<String> v1_sym;
Persistent<String> u2_sym;
Persistent<String> v2_sym;
Persistent<String> buf_sym;
Persistent<String> frames_sym;
Persistent<String> pts_sym;
Persistent<String> dts_sym;
Persistent<String> nals_sym;
Persistent<String> type_sym;
Persistent<String> priority_sym;
Persistent<String> offset_sym;
Persistent<String> size_sym;
Persistent<String> on_data_sym;
Persistent<String> on_error_sym;

fraction_t mach_timebase;


static Handle<Value> video_mixer_constructor(const Arguments &args)
{
    auto mixer = new video_mixer_platform();
    return mixer->init(args);
}

static void init(Handle<Object> e)
{
    kern_return_t k_ret;
    mach_timebase_info_data_t timebase;
    Handle<FunctionTemplate> func;

    NODE_DEFINE_CONSTANT(e, NAL_UNKNOWN);
    NODE_DEFINE_CONSTANT(e, NAL_SLICE);
    NODE_DEFINE_CONSTANT(e, NAL_SLICE_DPA);
    NODE_DEFINE_CONSTANT(e, NAL_SLICE_DPB);
    NODE_DEFINE_CONSTANT(e, NAL_SLICE_DPC);
    NODE_DEFINE_CONSTANT(e, NAL_SLICE_IDR);
    NODE_DEFINE_CONSTANT(e, NAL_SEI);
    NODE_DEFINE_CONSTANT(e, NAL_SPS);
    NODE_DEFINE_CONSTANT(e, NAL_PPS);
    NODE_DEFINE_CONSTANT(e, NAL_AUD);
    NODE_DEFINE_CONSTANT(e, NAL_FILLER);

    NODE_DEFINE_CONSTANT(e, NAL_PRIORITY_DISPOSABLE);
    NODE_DEFINE_CONSTANT(e, NAL_PRIORITY_LOW);
    NODE_DEFINE_CONSTANT(e, NAL_PRIORITY_HIGH);
    NODE_DEFINE_CONSTANT(e, NAL_PRIORITY_HIGHEST);

    buffer_size_sym = NODE_PSYMBOL("bufferSize");
    width_sym = NODE_PSYMBOL("width");
    height_sym = NODE_PSYMBOL("height");
    x264_preset_sym = NODE_PSYMBOL("x264Preset");
    x264_tuning_sym = NODE_PSYMBOL("x264Tuning");
    x264_params_sym = NODE_PSYMBOL("x264Params");
    x264_profile_sym = NODE_PSYMBOL("x264Profile");
    source_sym = NODE_PSYMBOL("source");
    x1_sym = NODE_PSYMBOL("x1");
    y1_sym = NODE_PSYMBOL("y1");
    x2_sym = NODE_PSYMBOL("x2");
    y2_sym = NODE_PSYMBOL("y2");
    u1_sym = NODE_PSYMBOL("u1");
    v1_sym = NODE_PSYMBOL("v1");
    u2_sym = NODE_PSYMBOL("u2");
    v2_sym = NODE_PSYMBOL("v2");
    buf_sym = NODE_PSYMBOL("buf");
    frames_sym = NODE_PSYMBOL("frames");
    pts_sym = NODE_PSYMBOL("pts");
    dts_sym = NODE_PSYMBOL("dts");
    nals_sym = NODE_PSYMBOL("nals");
    type_sym = NODE_PSYMBOL("type");
    priority_sym = NODE_PSYMBOL("priority");
    offset_sym = NODE_PSYMBOL("offset");
    size_sym = NODE_PSYMBOL("size");
    on_data_sym = NODE_PSYMBOL("onData");
    on_error_sym = NODE_PSYMBOL("onError");

    k_ret = mach_timebase_info(&timebase);
    if (k_ret != 0)
        abort();

    mach_timebase.num = timebase.numer;
    mach_timebase.den = timebase.denom;

    func = FunctionTemplate::New(video_mixer_constructor);
    func->InstanceTemplate()->SetInternalFieldCount(1);
    video_mixer_base::init_prototype(func);
    e->Set(String::NewSymbol("VideoMixer"), func->GetFunction());
}


} // namespace p1stream;

extern "C" void init(v8::Handle<v8::Object> e)
{
    p1stream::init(e);
}

NODE_MODULE(core, init)
