#include <v8.h>
#include <node.h>
#include <node_buffer.h>

namespace p1stream {

using namespace v8;
using namespace node;


static Persistent<Function> fast_buffer_constructor;


static size_t var_int(uint8_t *p, int64_t val, uint32_t maxlen)
{
    if (val < 0) {
        p[0] = 0xFF;
        return 1;
    }
    else if (val < 0x7F) {
        p[0] = val | 0x80;
        return 1;
    }
    else if (maxlen >= 2 && val < 0x3FFF) {
        p[0] = ((val & 0xFF00) >> 8) | 0x40;
        p[1] =  (val & 0x00FF);
        return 2;
    }
    else if (maxlen >= 3 && val < 0x1FFFFF) {
        p[0] = ((val & 0xFF0000) >> 16) | 0x20;
        p[1] =  (val & 0x00FF00) >> 8;
        p[2] =  (val & 0x0000FF);
        return 3;
    }
    else if (maxlen >= 4 && val < 0x0FFFFFFF) {
        p[0] = ((val & 0xFF000000) >> 24) | 0x10;
        p[1] =  (val & 0x00FF0000) >> 16;
        p[2] =  (val & 0x0000FF00) >> 8;
        p[3] =  (val & 0x000000FF);
        return 4;
    }
    else if (maxlen >= 5 && val < 0x07FFFFFFFF) {
        p[0] = ((val & 0xFF00000000) >> 32) | 0x08;
        p[1] =  (val & 0x00FF000000) >> 24;
        p[2] =  (val & 0x0000FF0000) >> 16;
        p[3] =  (val & 0x000000FF00) >> 8;
        p[4] =  (val & 0x00000000FF);
        return 5;
    }
    else if (maxlen >= 6 && val < 0x03FFFFFFFFFF) {
        p[0] = ((val & 0xFF0000000000) >> 40) | 0x04;
        p[1] =  (val & 0x00FF00000000) >> 32;
        p[2] =  (val & 0x0000FF000000) >> 24;
        p[3] =  (val & 0x000000FF0000) >> 16;
        p[4] =  (val & 0x00000000FF00) >> 8;
        p[5] =  (val & 0x0000000000FF);
        return 6;
    }
    else if (maxlen >= 7 && val < 0x01FFFFFFFFFFFF) {
        p[0] = ((val & 0xFF000000000000) >> 48) | 0x02;
        p[1] =  (val & 0x00FF0000000000) >> 40;
        p[2] =  (val & 0x0000FF00000000) >> 32;
        p[3] =  (val & 0x000000FF000000) >> 24;
        p[4] =  (val & 0x00000000FF0000) >> 16;
        p[5] =  (val & 0x0000000000FF00) >> 8;
        p[6] =  (val & 0x000000000000FF);
        return 7;
    }
    else if (maxlen == 8 && val < 0x00FFFFFFFFFFFFFF) {
        p[0] = 0x01;
        p[1] =  (val & 0xFF000000000000) >> 48;
        p[2] =  (val & 0x00FF0000000000) >> 40;
        p[3] =  (val & 0x0000FF00000000) >> 32;
        p[4] =  (val & 0x000000FF000000) >> 24;
        p[5] =  (val & 0x00000000FF0000) >> 16;
        p[6] =  (val & 0x0000000000FF00) >> 8;
        p[7] =  (val & 0x000000000000FF);
        return 8;
    }
    return 0;
}


static Handle<Value> var_int(const Arguments &args)
{
    if (args.Length() != 2 || !args[0]->IsNumber() || !args[1]->IsUint32())
        return ThrowException(Exception::TypeError(
                    String::New("Invalid arguments")));

    double val = args[0]->NumberValue();
    uint32_t maxlen = args[1]->Uint32Value();
    if (maxlen < 1 || maxlen > 8)
        return ThrowException(Exception::Error(
                    String::New("Invalid maxlen")));

    uint8_t buf[maxlen];
    size_t len = var_int(buf, val, maxlen);
    if (len == 0)
        return ThrowException(Exception::Error(
                    String::New("Could not encode varint")));

    Handle<Value> arg = Number::New(len);
    auto bufobj = fast_buffer_constructor->NewInstance(1, &arg);
    auto dst = Buffer::Data(bufobj);
    memcpy(dst, buf, len);

    return bufobj;
}


static void init(Handle<Object> e)
{
    Handle<FunctionTemplate> func;

    auto global = Context::GetCurrent()->Global();
    auto val = global->Get(String::NewSymbol("Buffer"));
    auto fn = Handle<Function>::Cast(val);
    fast_buffer_constructor = Persistent<Function>::New(fn);

    func = FunctionTemplate::New(var_int);
    e->Set(String::NewSymbol("varInt"), func->GetFunction());
}


} // namespace p1stream;

extern "C" void init(v8::Handle<v8::Object> e)
{
    p1stream::init(e);
}

NODE_MODULE(api, init)
