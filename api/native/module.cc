#include <v8.h>
#include <node.h>
#include <node_buffer.h>

namespace p1stream {

using namespace v8;
using namespace node;

static size_t calc_varint_size(uint64_t val);
static size_t write_varint(uint64_t val, uint8_t *p);
static Handle<Value> calc_ebml_content_size(char type, Handle<Value> contentVal, size_t &size);
static Handle<Value> calc_ebml_size(Handle<Value> val, size_t &size);
static Handle<Value> write_ebml(Handle<Value> tags, uint8_t *dst);
static Handle<Value> build_ebml(const Arguments &args);

static Persistent<Function> fast_buffer_constructor;


static size_t calc_varint_size(uint64_t val)
{
    if (val < 0x7F) return 1;
    else if (val < 0x3FFF) return 2;
    else if (val < 0x1FFFFF) return 3;
    else if (val < 0x0FFFFFFF) return 4;
    else if (val < 0x07FFFFFFFF) return 5;
    else if (val < 0x03FFFFFFFFFF) return 6;
    else if (val < 0x01FFFFFFFFFFFF) return 7;
    else return 8;
}


static size_t write_varint(uint64_t val, uint8_t *p)
{
    if (val < 0x7F) {
        p[0] = val | 0x80;
        return 1;
    }
    else if (val < 0x3FFF) {
        p[0] = ((val & 0xFF00) >> 8) | 0x40;
        p[1] =  (val & 0x00FF);
        return 2;
    }
    else if (val < 0x1FFFFF) {
        p[0] = ((val & 0xFF0000) >> 16) | 0x20;
        p[1] =  (val & 0x00FF00) >> 8;
        p[2] =  (val & 0x0000FF);
        return 3;
    }
    else if (val < 0x0FFFFFFF) {
        p[0] = ((val & 0xFF000000) >> 24) | 0x10;
        p[1] =  (val & 0x00FF0000) >> 16;
        p[2] =  (val & 0x0000FF00) >> 8;
        p[3] =  (val & 0x000000FF);
        return 4;
    }
    else if (val < 0x07FFFFFFFF) {
        p[0] = ((val & 0xFF00000000) >> 32) | 0x08;
        p[1] =  (val & 0x00FF000000) >> 24;
        p[2] =  (val & 0x0000FF0000) >> 16;
        p[3] =  (val & 0x000000FF00) >> 8;
        p[4] =  (val & 0x00000000FF);
        return 5;
    }
    else if (val < 0x03FFFFFFFFFF) {
        p[0] = ((val & 0xFF0000000000) >> 40) | 0x04;
        p[1] =  (val & 0x00FF00000000) >> 32;
        p[2] =  (val & 0x0000FF000000) >> 24;
        p[3] =  (val & 0x000000FF0000) >> 16;
        p[4] =  (val & 0x00000000FF00) >> 8;
        p[5] =  (val & 0x0000000000FF);
        return 6;
    }
    else if (val < 0x01FFFFFFFFFFFF) {
        p[0] = ((val & 0xFF000000000000) >> 48) | 0x02;
        p[1] =  (val & 0x00FF0000000000) >> 40;
        p[2] =  (val & 0x0000FF00000000) >> 32;
        p[3] =  (val & 0x000000FF000000) >> 24;
        p[4] =  (val & 0x00000000FF0000) >> 16;
        p[5] =  (val & 0x0000000000FF00) >> 8;
        p[6] =  (val & 0x000000000000FF);
        return 7;
    }
    else {
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
}


static Handle<Value> calc_ebml_content_size(char type, Handle<Value> contentVal, size_t &size)
{
    switch (type) {
        case 'm': {
            size = 0;
            return calc_ebml_size(contentVal, size);
        }
        case 'u': {
            double number = contentVal->NumberValue();
            if (number < 0)
                return ThrowException(Exception::TypeError(
                            String::New("Invalid unsigned value")));
            else if (number <= 0xFF) size = 1;
            else if (number <= 0xFFFF) size = 2;
            else if (number <= 0xFFFFFFFF) size = 4;
            else if (number <= 0xFFFFFFFFFFFFFFFF) size = 8;
            else
                return ThrowException(Exception::TypeError(
                            String::New("Invalid unsigned value")));
            break;
        }
        case 'f': {
            size = 4;
            break;
        }
        case 'F': {  // double
            size = 8;
            break;
        }
        case 's':
        case '8': {
            size = contentVal->ToString()->Utf8Length();
            break;
        }
        case 'b': {
            if (!Buffer::HasInstance(contentVal))
                return ThrowException(Exception::TypeError(
                            String::New("Invalid buffer value")));
            size = Buffer::Length(contentVal);
            break;
        }
        default:
            return ThrowException(Exception::TypeError(
                        String::New("Invalid tag type")));
    }

    return Handle<Value>();
}


static Handle<Value> calc_ebml_size(Handle<Value> val, size_t &size)
{
    if (!val->IsArray())
        return ThrowException(Exception::TypeError(
                    String::New("Expected an array of tags")));

    auto tags = Handle<Array>::Cast(val);
    uint32_t length = tags->Length();
    for (uint32_t i = 0; i < length; i++) {
        auto tagVal = tags->Get(i);
        if (!tagVal->IsArray())
            return ThrowException(Exception::TypeError(
                        String::New("Invalid tag")));

        auto tag = Handle<Array>::Cast(tagVal);
        auto tagLength = tag->Length();
        if (tagLength < 3 || tagLength > 4)
            return ThrowException(Exception::TypeError(
                        String::New("Invalid tag")));

        auto idVal = tag->Get(0);
        auto typeVal = tag->Get(1);
        auto contentVal = tag->Get(2);

        if (!Buffer::HasInstance(idVal))
            return ThrowException(Exception::TypeError(
                        String::New("Invalid tag ID")));

        auto idSize = Buffer::Length(idVal);
        if (idSize < 1 || idSize > 4)
            return ThrowException(Exception::TypeError(
                        String::New("Invalid tag ID")));

        String::AsciiValue typeStr(typeVal);
        if (typeStr.length() != 1)
            return ThrowException(Exception::TypeError(
                        String::New("Invalid tag type")));

        size_t contentSize;
        auto ret = calc_ebml_content_size(**typeStr, contentVal, contentSize);
        if (!ret.IsEmpty())
            return ret;

        size_t sizeSize = tag->Get(3)->IsTrue() ? 1 : calc_varint_size(contentSize);

        size += idSize + sizeSize + contentSize;
    }

    return Handle<Value>();
}

static Handle<Value> write_ebml(Handle<Value> val, uint8_t *dst)
{
    auto tags = Handle<Array>::Cast(val);
    uint32_t length = tags->Length();
    for (uint32_t i = 0; i < length; i++) {
        auto tag = Handle<Array>::Cast(tags->Get(i));

        auto idVal = tag->Get(0);
        auto typeVal = tag->Get(1);
        auto contentVal = tag->Get(2);

        auto idSize = Buffer::Length(idVal);
        memcpy(dst, Buffer::Data(idVal), idSize);
        dst += idSize;

        size_t contentSize;
        String::AsciiValue typeStr(typeVal);
        auto ret = calc_ebml_content_size(**typeStr, contentVal, contentSize);
        if (!ret.IsEmpty())
            return ret;

        if (tag->Get(3)->IsTrue()) {
            *dst = 0xFF;
            dst += 1;
        }
        else {
            dst += write_varint(contentSize, dst);
        }

        switch (**typeStr) {
            case 'm': {
                auto ret = write_ebml(contentVal, dst);
                if (!ret.IsEmpty())
                    return ret;
                break;
            }
            case 'u': {
                uint64_t number = contentVal->NumberValue();
                auto *p = (uint8_t *) &number;
                if (number <= 0xFF) {
                    dst[0] = p[0];
                }
                else if (number <= 0xFFFF) {
                    dst[0] = p[1];
                    dst[1] = p[0];
                }
                else if (number <= 0xFFFFFFFF) {
                    dst[0] = p[3];
                    dst[1] = p[2];
                    dst[2] = p[1];
                    dst[3] = p[0];
                }
                else {
                    dst[0] = p[7];
                    dst[1] = p[6];
                    dst[2] = p[5];
                    dst[3] = p[4];
                    dst[4] = p[3];
                    dst[5] = p[2];
                    dst[6] = p[1];
                    dst[7] = p[0];
                }
                break;
            }
            case 'f': {
                float number = contentVal->NumberValue();
                auto *p = (uint8_t *) &number;
                dst[0] = p[3];
                dst[1] = p[2];
                dst[2] = p[1];
                dst[3] = p[0];
                break;
            }
            case 'F': {  // double
                double number = contentVal->NumberValue();
                auto *p = (uint8_t *) &number;
                dst[0] = p[7];
                dst[1] = p[6];
                dst[2] = p[5];
                dst[3] = p[4];
                dst[4] = p[3];
                dst[5] = p[2];
                dst[6] = p[1];
                dst[7] = p[0];
                break;
            }
            case 's':
            case '8': {
                contentVal->ToString()->WriteUtf8((char *) dst, -1, NULL,
                        String::NO_NULL_TERMINATION | String::PRESERVE_ASCII_NULL);
                break;
            }
            case 'b': {
                memcpy(dst, Buffer::Data(contentVal), contentSize);
                break;
            }
        }

        dst += contentSize;
    }

    return Handle<Value>();
}


static Handle<Value> build_ebml(const Arguments &args)
{
    HandleScope scope;
    Handle<Value> ret;

    if (args.Length() != 1)
        return ThrowException(Exception::TypeError(
                    String::New("Expected one argument")));

    size_t size = 0;
    ret = calc_ebml_size(args[0], size);
    if (!ret.IsEmpty())
        return ret;

    Handle<Value> arg = Number::New(size);
    auto bufobj = fast_buffer_constructor->NewInstance(1, &arg);
    uint8_t *dst = (uint8_t *) Buffer::Data(bufobj);

    ret = write_ebml(args[0], dst);
    if (!ret.IsEmpty())
        return ret;

    return scope.Close(bufobj);
}


static void init(Handle<Object> e)
{
    Handle<FunctionTemplate> func;

    auto global = Context::GetCurrent()->Global();
    auto val = global->Get(String::NewSymbol("Buffer"));
    auto fn = Handle<Function>::Cast(val);
    fast_buffer_constructor = Persistent<Function>::New(fn);

    func = FunctionTemplate::New(build_ebml);
    e->Set(String::NewSymbol("buildEBML"), func->GetFunction());
}


} // namespace p1stream;

extern "C" void init(v8::Handle<v8::Object> e)
{
    p1stream::init(e);
}

NODE_MODULE(api, init)
