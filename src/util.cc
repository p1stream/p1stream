#include "p1stream_priv.h"
#include "node_buffer.h"

namespace p1stream {


void lockable::unlock()
{
}

lockable *lockable_mutex::lock()
{
    uv_mutex_lock(&mutex);
    return this;
}

void lockable_mutex::unlock()
{
    uv_mutex_unlock(&mutex);
}

void threaded_loop::thread_cb(void *arg)
{
    auto &loop = *((threaded_loop *) arg);
    lock_handle lock(loop);
    loop.fn();
}

void async::signal_cb(uv_async_t *handle)
{
    auto &fn = ((ctx *) handle)->fn;
    if (fn != nullptr)
        fn();
}

void async::close_cb(uv_handle_t *handle)
{
    delete (ctx *) handle;
}

buffer_slicer::buffer_slicer(Local<Object> buffer) :
    buffer_(buffer)
{
    isolate_ = buffer->GetIsolate();
    buffer_proto_ = buffer->GetPrototype();
    length_sym_ = length_sym.Get(isolate_);
    parent_sym_ = parent_sym.Get(isolate_);
}

Local<Object> buffer_slicer::slice(char *data, uint32_t length)
{
    // Roughly emulates Buffer::New and slice().
    // FIXME: Get a native slice in io.js that uses the constructor.
    auto obj = Object::New(isolate_);
    obj->SetPrototype(buffer_proto_);
    obj->Set(length_sym_, Uint32::NewFromUnsigned(isolate_, length));
    obj->Set(parent_sym_, buffer_);
    obj->SetIndexedPropertiesToExternalArrayData(data, v8::kExternalUnsignedByteArray, length);
    return obj;
}

event_buffer::event_buffer(lockable *lock, event_transform transform, int size) :
    size_(size), used_(0), stalled_(0), lock_(lock), transform_(transform),
    async_(std::bind(&event_buffer::flush, this))
{
    data_ = new char[size];
}

event_buffer::~event_buffer()
{
    callback_.Reset();
    context_.Reset();
    delete[] data_;
}

void event_buffer::set_callback(v8::Handle<v8::Context> context, v8::Handle<v8::Function> callback)
{
    isolate_ = context->GetIsolate();
    context_.Reset(isolate_, context);
    callback_.Reset(isolate_, callback);
}

void event_buffer::flush()
{
    HandleScope handle_scope(isolate_);

    auto callback = Local<Function>::New(isolate_, callback_);
    if (callback.IsEmpty())
        return;

    int used;
    int stalled;
    char *data;
    {
        lock_handle lock(lock_);

        used = used_;
        stalled = stalled_;

        used_ = 0;
        stalled_ = 0;

        if (!used)
            return;

        data = (char *) malloc(used);
        if (data == NULL)
            return;

        memcpy(data, data_, used);
    }

    auto context = Local<Context>::New(isolate_, context_);
    auto global = context->Global();
    buffer_slicer slicer(Buffer::Use(isolate_, data, used));
    Context::Scope context_scope(context);

    auto *end = (event *) (data + used);
    auto *ev = (event *) data;
    while (ev < end) {
        Handle<Value> args[2];
        args[0] = Uint32::NewFromUnsigned(isolate_, ev->id);
        switch (ev->id) {
            case EV_LOG_TRACE:
            case EV_LOG_DEBUG:
            case EV_LOG_INFO:
            case EV_LOG_WARN:
            case EV_LOG_ERROR:
            case EV_LOG_FATAL:
                args[1] = String::NewFromUtf8(isolate_, ev->data, String::kNormalString, ev->size);
                break;
            case EV_FAILURE:
            case EV_STALLED:
                args[1] = Undefined(isolate_);
                break;
            default:
                if (transform_)
                    args[1] = transform_(isolate_, *ev, slicer);
                else
                    args[1] = Undefined(isolate_);
                break;
        }
        MakeCallback(isolate_, global, callback, 2, args);
        ev = (event *) ((char *) ev + ev->total_size());
    }

    if (stalled) {
        MakeCallback(isolate_, global, callback, 2, (Handle<Value>[]) {
            Uint32::NewFromUnsigned(isolate_, EV_STALLED),
            Integer::New(isolate_, stalled)
        });
    }
}


} // namespace p1stream
