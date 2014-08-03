#include "p1stream_priv.h"

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


} // namespace p1stream
