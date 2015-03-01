#include "p1stream_priv.h"

namespace p1stream {


void software_clock::init(const FunctionCallbackInfo<Value>& args)
{
    auto *isolate = args.GetIsolate();

    if (args.Length() != 1 || !args[0]->IsObject()) {
        isolate->ThrowException(Exception::TypeError(
            String::NewFromUtf8(isolate, "Expected an object")));
        return;
    }
    auto params = args[0].As<Object>();

    auto numVal = params->Get(numerator_sym.Get(isolate));
    auto denVal = params->Get(denominator_sym.Get(isolate));
    if (!numVal->IsUint32()) {
        isolate->ThrowException(Exception::TypeError(
            String::NewFromUtf8(isolate, "Invalid numerator")));
        return;
    }
    if (!denVal->IsUint32()) {
        isolate->ThrowException(Exception::TypeError(
            String::NewFromUtf8(isolate, "Invalid denominator")));
        return;
    }

    rate.num = numVal->Uint32Value();
    rate.den = denVal->Uint32Value();
    if (rate.num == 0 || rate.den == 0) {
        isolate->ThrowException(Exception::TypeError(
            String::NewFromUtf8(isolate, "Invalid fraction")));
        return;
    }

    // Parameters checked, from here on we no longer throw exceptions.
    Wrap(args.This());
    Ref();
    args.GetReturnValue().Set(handle());

    thread.init(std::bind(&software_clock::loop, this));
    running = true;
}

void software_clock::destroy()
{
    if (running) {
        thread.destroy();
        running = false;
    }

    Unref();
}

lockable *software_clock::lock()
{
    return thread.lock();
}

void software_clock::link_video_clock(video_clock_context &ctx)
{
    ctxes.push_back(&ctx);
}

void software_clock::unlink_video_clock(video_clock_context &ctx)
{
    ctxes.remove(&ctx);
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

        for (auto ctx : ctxes)
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


}  // namespace p1stream
