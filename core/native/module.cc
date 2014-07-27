#include "core_priv.h"

#if __APPLE__
#   include <TargetConditionals.h>
#   if TARGET_OS_MAC
#       include "core_priv_mac.h"
#       define video_mixer_platform video_mixer_mac
#   endif
#elif __linux
#   include "core_priv_linux.h"
#   define video_mixer_platform video_mixer_linux
#endif
#ifndef video_mixer_platform
#   error Platform not supported
#endif

namespace p1_core {


Eternal<String> source_sym;
Eternal<String> on_data_sym;
Eternal<String> on_error_sym;

Eternal<String> buffer_size_sym;
Eternal<String> width_sym;
Eternal<String> height_sym;
Eternal<String> x264_preset_sym;
Eternal<String> x264_tuning_sym;
Eternal<String> x264_params_sym;
Eternal<String> x264_profile_sym;
Eternal<String> clock_sym;
Eternal<String> x1_sym;
Eternal<String> y1_sym;
Eternal<String> x2_sym;
Eternal<String> y2_sym;
Eternal<String> u1_sym;
Eternal<String> v1_sym;
Eternal<String> u2_sym;
Eternal<String> v2_sym;
Eternal<String> buf_sym;
Eternal<String> frames_sym;
Eternal<String> pts_sym;
Eternal<String> dts_sym;
Eternal<String> keyframe_sym;
Eternal<String> nals_sym;
Eternal<String> type_sym;
Eternal<String> priority_sym;
Eternal<String> start_sym;
Eternal<String> end_sym;

Eternal<String> volume_sym;


static void video_mixer_constructor(const FunctionCallbackInfo<Value>& args)
{
    auto mixer = new video_mixer_platform();
    mixer->init(args);
}

static void audio_mixer_constructor(const FunctionCallbackInfo<Value>& args)
{
    auto mixer = new audio_mixer_full();
    mixer->init(args);
}

static void init(v8::Handle<v8::Object> exports, v8::Handle<v8::Value> module,
    v8::Handle<v8::Context> context, void* priv)
{
    auto *isolate = context->GetIsolate();
    Handle<String> name;
    Handle<FunctionTemplate> func;

    NODE_DEFINE_CONSTANT(exports, NAL_UNKNOWN);
    NODE_DEFINE_CONSTANT(exports, NAL_SLICE);
    NODE_DEFINE_CONSTANT(exports, NAL_SLICE_DPA);
    NODE_DEFINE_CONSTANT(exports, NAL_SLICE_DPB);
    NODE_DEFINE_CONSTANT(exports, NAL_SLICE_DPC);
    NODE_DEFINE_CONSTANT(exports, NAL_SLICE_IDR);
    NODE_DEFINE_CONSTANT(exports, NAL_SEI);
    NODE_DEFINE_CONSTANT(exports, NAL_SPS);
    NODE_DEFINE_CONSTANT(exports, NAL_PPS);
    NODE_DEFINE_CONSTANT(exports, NAL_AUD);
    NODE_DEFINE_CONSTANT(exports, NAL_FILLER);

    NODE_DEFINE_CONSTANT(exports, NAL_PRIORITY_DISPOSABLE);
    NODE_DEFINE_CONSTANT(exports, NAL_PRIORITY_LOW);
    NODE_DEFINE_CONSTANT(exports, NAL_PRIORITY_HIGH);
    NODE_DEFINE_CONSTANT(exports, NAL_PRIORITY_HIGHEST);

    // FIXME: Create our own environment, like node.
#define SYM(handle, value) handle.Set(isolate, String::NewFromUtf8(isolate, value))
    SYM(source_sym, "source");
    SYM(on_data_sym, "onData");
    SYM(on_error_sym, "onError");

    SYM(buffer_size_sym, "bufferSize");
    SYM(width_sym, "width");
    SYM(height_sym, "height");
    SYM(x264_preset_sym, "x264Preset");
    SYM(x264_tuning_sym, "x264Tuning");
    SYM(x264_params_sym, "x264Params");
    SYM(x264_profile_sym, "x264Profile");
    SYM(clock_sym, "clock");
    SYM(x1_sym, "x1");
    SYM(y1_sym, "y1");
    SYM(x2_sym, "x2");
    SYM(y2_sym, "y2");
    SYM(u1_sym, "u1");
    SYM(v1_sym, "v1");
    SYM(u2_sym, "u2");
    SYM(v2_sym, "v2");
    SYM(buf_sym, "buf");
    SYM(frames_sym, "frames");
    SYM(pts_sym, "pts");
    SYM(dts_sym, "dts");
    SYM(keyframe_sym, "keyframe");
    SYM(nals_sym, "nals");
    SYM(type_sym, "type");
    SYM(priority_sym, "priority");
    SYM(start_sym, "start");
    SYM(end_sym, "end");

    SYM(volume_sym, "volume");
#undef SYM

    name = String::NewFromUtf8(isolate, "VideoMixer");
    func = FunctionTemplate::New(isolate, video_mixer_constructor);
    func->InstanceTemplate()->SetInternalFieldCount(1);
    func->SetClassName(name);
    video_mixer_base::init_prototype(func);
    exports->Set(name, func->GetFunction());

    name = String::NewFromUtf8(isolate, "AudioMixer");
    func = FunctionTemplate::New(isolate, audio_mixer_constructor);
    func->InstanceTemplate()->SetInternalFieldCount(1);
    func->SetClassName(name);
    audio_mixer_full::init_prototype(func);
    exports->Set(name, func->GetFunction());

    module_platform_init();
}


} // namespace p1_core;

NODE_MODULE_CONTEXT_AWARE(core, p1_core::init)
