#include "core_priv.h"

namespace p1_core {


void software_clock::init(const FunctionCallbackInfo<Value>& args)
{
    auto *isolate = args.GetIsolate();
    bool ok;

    Wrap(args.This());
    args.GetReturnValue().Set(handle());

    if (!(ok = args.Length() == 1 && args[0]->IsObject())) {
        throw_type_error("Expected an object");
    }
    else {
        auto params = args[0].As<Object>();

        auto numVal = params->Get(numerator_sym.Get(isolate));
        auto denVal = params->Get(denominator_sym.Get(isolate));
        if (!(ok = numVal->IsUint32())) {
            throw_type_error("Invalid numerator");
        }
        else if (!(ok = denVal->IsUint32())) {
            throw_type_error("Invalid denominator");
        }
        else {
            rate.num = numVal->Uint32Value();
            rate.den = denVal->Uint32Value();
            if (!(ok = rate.num > 0 && rate.den > 0))
                throw_type_error("Invalid fraction");
        }
    }

    if (ok) {
        thread.init(std::bind(&software_clock::loop, this));
        running = true;

        Ref();
    }
    else {
        destroy(false);
    }
}

void software_clock::destroy(bool unref)
{
    if (running) {
        thread.destroy();
        running = false;
    }

    if (unref)
        Unref();
}

lockable *software_clock::lock()
{
    return thread.lock();
}

bool software_clock::link_video_clock(video_clock_context &ctx_)
{
    if (ctx != nullptr) {
        throw_error("SoftwareClock can only link to one mixer");
        return false;
    }

    ctx = &ctx_;
    return true;
}

void software_clock::unlink_video_clock(video_clock_context &ctx_)
{
    if (ctx == &ctx_)
        ctx = nullptr;
}

fraction_t software_clock::video_ticks_per_second(video_clock_context &ctx)
{
    return rate;
}

void software_clock::loop()
{
    int64_t interval = 1000000000 * rate.num / rate.den;
    int64_t time_now = system_time();
    int64_t time_next = time_now + interval;

    do {
        if (thread.wait(time_next - time_now))
            break;

        if (ctx)
            ctx->tick(time_next);

        time_now = system_time();
        while (time_next < time_now)
            time_next += interval;
    }
    while (true);
}

void software_clock::init_prototype(Handle<FunctionTemplate> func)
{
    NODE_SET_PROTOTYPE_METHOD(func, "destroy", [](const FunctionCallbackInfo<Value>& args) {
        auto clock = ObjectWrap::Unwrap<software_clock>(args.This());
        clock->destroy();
    });
}


}  // namespace p1_core
