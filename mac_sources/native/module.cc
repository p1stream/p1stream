#include "mac_sources_priv.h"

namespace p1stream {


Persistent<String> display_id_sym;
Persistent<String> divisor_sym;


static Handle<Value> display_link_constructor(const Arguments &args)
{
    auto link = new display_link();
    return link->init(args);
}

static Handle<Value> display_stream_constructor(const Arguments &args)
{
    auto link = new display_stream();
    return link->init(args);
}

static void init(Handle<Object> e)
{
    Handle<FunctionTemplate> func;

    display_id_sym = NODE_PSYMBOL("displayId");
    divisor_sym = NODE_PSYMBOL("divisor");

    func = FunctionTemplate::New(display_link_constructor);
    func->InstanceTemplate()->SetInternalFieldCount(1);
    display_link::init_prototype(func);
    e->Set(String::NewSymbol("DisplayLink"), func->GetFunction());

    func = FunctionTemplate::New(display_stream_constructor);
    func->InstanceTemplate()->SetInternalFieldCount(1);
    display_link::init_prototype(func);
    e->Set(String::NewSymbol("DisplayStream"), func->GetFunction());
}


} // namespace p1stream;

extern "C" void init(v8::Handle<v8::Object> e)
{
    p1stream::init(e);
}

NODE_MODULE(mac_sources, init)
