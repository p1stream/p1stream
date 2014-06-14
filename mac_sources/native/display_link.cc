#include "mac_sources_priv.h"

namespace p1stream {


Handle<Value> display_link::init(const Arguments &args)
{
    bool ok = true;
    Handle<Value> ret;
    char err[128];

    CVReturn cv_ret;

    Handle<Object> params;
    Handle<Value> val;

    Wrap(args.This());

    display_id = kCGDirectMainDisplay;
    divisor = 1;

    if (args.Length() == 1) {
        if (!(ok = args[0]->IsObject()))
            ret = Exception::TypeError(
                String::New("Expected an object"));
        else
            params = Local<Object>::Cast(args[0]);

        if (ok) {
            val = params->Get(display_id_sym);
            if (!val->IsUndefined()) {
                if (!(ok = val->IsUint32()))
                    ret = Exception::TypeError(
                        String::New("Invalid display ID"));
                else
                    display_id = val->Uint32Value();
            }
        }

        if (ok) {
            val = params->Get(divisor_sym);
            if (!val->IsUndefined()) {
                if (val->IsUint32())
                    divisor = val->Uint32Value();
                else
                    divisor = 0;

                if (!(ok = (divisor >= 1)))
                    ret = Exception::TypeError(
                        String::New("Invalid divisor value"));
            }
        }
    }

    if (ok) {
        cv_ret = CVDisplayLinkCreateWithCGDisplay(display_id, &cv_handle);
        if (!(ok = (cv_ret == kCVReturnSuccess)))
            sprintf(err, "CVDisplayLinkCreateWithCGDisplay error %d", cv_ret);
    }

    if (ok) {
        cv_ret = CVDisplayLinkSetOutputCallback(cv_handle, callback, this);
        if (!(ok = (cv_ret == kCVReturnSuccess)))
            sprintf(err, "CVDisplayLinkSetOutputCallback error %d", cv_ret);
    }

    if (ok) {
        cv_ret = CVDisplayLinkStart(cv_handle);
        if (!(ok = (cv_ret == kCVReturnSuccess)))
            sprintf(err, "CVDisplayLinkStart error %d", cv_ret);
        else
            running = true;
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

void display_link::destroy(bool unref)
{
    CVReturn cv_ret;

    if (running) {
        cv_ret = CVDisplayLinkStop(cv_handle);
        if (cv_ret != kCVReturnSuccess)
            fprintf(stderr, "CVDisplayLinkStop error %d", cv_ret);
        running = false;
    }

    if (cv_handle != NULL) {
        CFRelease(cv_handle);
        cv_handle = NULL;
    }

    if (unref)
        Unref();
}

fraction_t display_link::ticks_per_second()
{
    double period = CVDisplayLinkGetActualOutputVideoRefreshPeriod(cv_handle);
    if (period == 0.0) {
        return {
            .num = 0,
            .den = 0
        };
    }
    else {
        return {
            .num = (uint32_t) round(1.0 / period),
            .den = divisor
        };
    }
}

void display_link::ref_mixer(video_mixer *mixer_)
{
    if (mixer == nullptr) {
        mixer = mixer_;
    }
    else {
        // FIXME: error
    }
}

void display_link::unref_mixer(video_mixer *mixer_)
{
    if (mixer == mixer_) {
        mixer = nullptr;
    }
    else {
        // FIXME: error
    }
}

CVReturn display_link::callback(
    CVDisplayLinkRef cv_handle,
    const CVTimeStamp *now,
    const CVTimeStamp *output_time,
    CVOptionFlags flags_in,
    CVOptionFlags *flags_out,
    void *context)
{
    auto *link = (display_link *) context;
    link->tick(now->hostTime);
    return kCVReturnSuccess;
}

void display_link::tick(frame_time_t time)
{
    // Skip tick based on divisor.
    if (skip_counter == divisor)
        skip_counter = 0;
    if (skip_counter++ != 0)
        return;

    // Call mixer with lock.
    {
        lock_handle lock(this);
        if (mixer != nullptr)
            mixer->tick(time);
    }
}

void display_link::init_prototype(Handle<FunctionTemplate> func)
{
    SetPrototypeMethod(func, "destroy", [](const Arguments &args) -> Handle<Value> {
        auto link = ObjectWrap::Unwrap<display_link>(args.This());
        link->destroy();
        return Undefined();
    });
}


}  // namespace p1stream