#include "p1stream_priv_linux.h"

namespace p1stream {


int64_t system_time()
{
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t) != 0)
        abort();
    return t.tv_sec * (int64_t) 1e9 + t.tv_nsec;
}

void module_platform_init(
    Handle<Object> exports, Handle<Value> module,
    Handle<Context> context, void* priv);
{
}


} // namespace p1stream
