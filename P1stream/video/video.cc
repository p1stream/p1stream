#include <v8.h>
#include <node.h>

using namespace v8;
using namespace node;

extern "C" void
init(Handle<Object> target)
{
}

NODE_MODULE(video, init)
