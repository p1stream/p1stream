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


static v8::Handle<v8::Value> video_mixer_constructor(const v8::Arguments &args)
{
    auto mixer = new p1stream::video_mixer_platform();
    return mixer->init(args);
}

extern "C" void
init(v8::Handle<v8::Object> exports)
{
    auto func = v8::FunctionTemplate::New(video_mixer_constructor);
    func->InstanceTemplate()->SetInternalFieldCount(1);
    p1stream::video_mixer_base::init_prototype(func);
    exports->Set(v8::String::NewSymbol("VideoMixer"), func->GetFunction());
}

NODE_MODULE(core, init)
