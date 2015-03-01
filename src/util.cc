#include "p1stream_priv.h"
#include "node_buffer.h"

namespace p1stream {

static void ebc_free_callback(char *data, void *hint);


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

void main_loop_callback::async_cb(uv_async_t *handle)
{
    auto *callback = (main_loop_callback *) handle;
    callback->fn();
}

void threaded_loop::thread_cb(void *arg)
{
    auto &loop = *((threaded_loop *) arg);
    lock_handle lock(loop);
    loop.fn();
}

void event_buffer_copy::callback(
    Isolate *isolate, Handle<Object> recv, Handle<Function> fn,
    Local<Value> (*transform)(Isolate *isolate, event &ev, buffer_slicer &slicer)
)
{
    buffer_slicer slicer(Buffer::New(isolate, data, size, ebc_free_callback, this));

    auto *end = (event *) (data + size);
    for (auto *ev = (event *) data; ev < end; ev = ev->next()) {
        Handle<Value> args[2];
        args[0] = Uint32::NewFromUnsigned(isolate, ev->id);
        switch (ev->id) {
            case EV_LOG_TRACE:
            case EV_LOG_DEBUG:
            case EV_LOG_INFO:
            case EV_LOG_WARN:
            case EV_LOG_ERROR:
            case EV_LOG_FATAL:
                args[1] = String::NewFromUtf8(isolate, ev->data, String::kNormalString, ev->size);
                break;
            case EV_FAILURE:
            case EV_STALLED:
                args[1] = Undefined(isolate);
                break;
            default:
                args[1] = transform(isolate, *ev, slicer);
                break;
        }
        MakeCallback(isolate, recv, fn, 2, args);
    }

    if (stalled) {
        MakeCallback(isolate, recv, fn, 2, (Handle<Value>[]) {
            Integer::NewFromUnsigned(isolate, EV_STALLED),
            Integer::New(isolate, stalled)
        });
    }
}

buffer_slicer::buffer_slicer(Local<Object> buffer)
    : buffer_(buffer)
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

static void ebc_free_callback(char *data, void *hint)
{
    // Match event_buffer::copy_out
    auto *mem = (char *) hint;
    delete[] mem;
}


} // namespace p1stream
