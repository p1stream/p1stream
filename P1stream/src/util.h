#ifndef p1_util_h
#define p1_util_h

#include <node.h>

namespace p1stream {

inline Handle<Value> syscall_error(const char *name, int code)
{
    char msg[128];
    snprintf(msg, 128, "%s error %d", name, code);
    return Exception::Error(String::New(msg));
}

}  // namespace p1stream

#endif  // p1_util_h
