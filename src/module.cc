#include "module.h"

#if __APPLE__
#   include <TargetConditionals.h>
#   if TARGET_OS_MAC
#       include "video_mac.h"
#       define video_mixer_platform video_mixer_mac
#   endif
#endif
#ifndef video_mixer_platform
#   error Platform not supported
#endif

namespace p1stream {


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
Persistent<String> data_sym;
Persistent<String> type_sym;
Persistent<String> offset_sym;
Persistent<String> size_sym;
Persistent<String> on_frame_sym;
Persistent<String> on_error_sym;

static Handle<Value> video_mixer_constructor(const Arguments &args)
{
    auto mixer = new video_mixer_platform();
    return mixer->init(args);
}

static void init(Handle<Object> exports)
{
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
    data_sym = NODE_PSYMBOL("data");
    type_sym = NODE_PSYMBOL("type");
    offset_sym = NODE_PSYMBOL("offset");
    size_sym = NODE_PSYMBOL("size");
    on_frame_sym = NODE_PSYMBOL("onFrame");
    on_error_sym = NODE_PSYMBOL("onError");

    auto func = FunctionTemplate::New(video_mixer_constructor);
    func->InstanceTemplate()->SetInternalFieldCount(1);
    video_mixer_base::init_prototype(func);
    exports->Set(String::NewSymbol("VideoMixer"), func->GetFunction());
}


} // namespace p1stream;

extern "C" void init(v8::Handle<v8::Object> exports)
{
    p1stream::init(exports);
}

NODE_MODULE(core, init)
