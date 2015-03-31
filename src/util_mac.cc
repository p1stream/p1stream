#include "p1stream_priv_mac.h"

#include <mach/mach_time.h>
#include <mach/mach_error.h>
#include <CoreFoundation/CoreFoundation.h>

namespace p1stream {


struct main_loop_context {
    uv_loop_t *loop;
    CFRunLoopRef cf_loop_ref;
    CFRunLoopSourceRef source_ref;
    bool source_active;
};

static Eternal<String> exit_code_sym;
static Eternal<String> before_exit_sym;

static mach_timebase_info_data_t timebase;

static void main_loop(
    const FunctionCallbackInfo<Value>& args);
static bool main_loop_update_uv(
    main_loop_context &ctx);
static void main_loop_uv_callback(
    CFFileDescriptorRef fd_ref, CFOptionFlags flags, void *info);
static void main_loop_emit_before_exit(
    Isolate *isolate, Handle<Context> context, Handle<Object> process);


int64_t system_time()
{
    return mach_absolute_time() * timebase.numer / timebase.denom;
}

void module_platform_init(
    Handle<Object> exports, Handle<Value> module,
    Handle<Context> context, void* priv)
{
    auto *isolate = context->GetIsolate();

#define SYM(handle, value) handle.Set(isolate, String::NewFromUtf8(isolate, value))
    SYM(exit_code_sym, "exitCode");
    SYM(before_exit_sym, "beforeExit");
#undef SYM

    auto name = String::NewFromUtf8(isolate, "mainLoop");
    auto func = FunctionTemplate::New(isolate, main_loop);
    exports->Set(name, func->GetFunction());

    auto k_ret = mach_timebase_info(&timebase);
    if (k_ret != KERN_SUCCESS)
        abort();
}

static void main_loop(
    const FunctionCallbackInfo<Value>& args)
{
    auto isolate = args.GetIsolate();
    auto context = isolate->GetCurrentContext();
    auto process = args[0].As<Object>();

    main_loop_context ctx;
    ctx.cf_loop_ref = CFRunLoopGetCurrent();
    ctx.loop = uv_default_loop();

    CFFileDescriptorContext fd_ctx;
    memset(&fd_ctx, 0, sizeof(fd_ctx));
    fd_ctx.info = &ctx;

    auto fd_ref = CFFileDescriptorCreate(kCFAllocatorDefault,
        uv_backend_fd(ctx.loop), false, main_loop_uv_callback, &fd_ctx);
    CFFileDescriptorEnableCallBacks(fd_ref, kCFFileDescriptorReadCallBack);
    ctx.source_ref = CFFileDescriptorCreateRunLoopSource(kCFAllocatorDefault, fd_ref, 0);

    ctx.source_active = false;
    main_loop_update_uv(ctx);

    while (true) {
        CFTimeInterval timeout = ctx.source_active ?
            (CFTimeInterval) uv_backend_timeout(ctx.loop) / 1000 : 60;
        SInt32 rl_ret = CFRunLoopRunInMode(kCFRunLoopDefaultMode, timeout, true);

        if (rl_ret == kCFRunLoopRunStopped) {
            break;
        }
        else if (rl_ret == kCFRunLoopRunFinished) {
            main_loop_emit_before_exit(isolate, context, process);
            main_loop_update_uv(ctx);

            rl_ret = CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, false);
            if (rl_ret == kCFRunLoopRunStopped || rl_ret == kCFRunLoopRunFinished)
                break;
        }
        else if (rl_ret == kCFRunLoopRunTimedOut && ctx.source_active) {
            main_loop_update_uv(ctx);
        }
    }

    CFRelease(ctx.cf_loop_ref);
    CFRelease(ctx.source_ref);
    CFRelease(fd_ref);
}

static bool main_loop_update_uv(main_loop_context &ctx)
{
    bool was_active = ctx.source_active;
    bool is_active = uv_run(ctx.loop, UV_RUN_NOWAIT) != 0;

    if (!was_active && is_active)
        CFRunLoopAddSource(ctx.cf_loop_ref, ctx.source_ref, kCFRunLoopCommonModes);
    else if (was_active && !is_active)
        CFRunLoopRemoveSource(ctx.cf_loop_ref, ctx.source_ref, kCFRunLoopCommonModes);

    ctx.source_active = is_active;
    return is_active;
}

static void main_loop_uv_callback(
    CFFileDescriptorRef fd_ref, CFOptionFlags flags, void *info)
{
    main_loop_update_uv(*(main_loop_context *) info);
    CFFileDescriptorEnableCallBacks(fd_ref, kCFFileDescriptorReadCallBack);
}

static void main_loop_emit_before_exit(
    Isolate *isolate, Handle<Context> context, Handle<Object> process)
{
    HandleScope scope(isolate);
    Context::Scope context_scope(context);

    MakeCallback(isolate, process, "emit", 2, (Local<Value>[]) {
        before_exit_sym.Get(isolate),
        process->Get(exit_code_sym.Get(isolate))->ToInteger(isolate)
    });
}


} // namespace p1stream
