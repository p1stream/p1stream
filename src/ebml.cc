#include "p1stream_priv.h"

#include <memory.h>
#include <v8.h>
#include <node.h>
#include <node_buffer.h>

namespace p1stream {

using namespace v8;
using namespace node;


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


static inline void throw_error(Isolate *isolate, const char *msg)
{
    isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, msg)));
}


static bool calc_ebml_size(Isolate *isolate, Handle<Value> val, size_t &size);

static bool calc_ebml_content_size(Isolate *isolate, char type, Handle<Value> contentVal, size_t &size)
{
    switch (type) {
        case 'm':
            return calc_ebml_size(isolate, contentVal, size);
        case 'u': {
            double number = contentVal->NumberValue();
            if (number < 0) {
                throw_error(isolate, "Invalid unsigned value");
                return false;
            }
            else if (number <= 0xFF) size = 1;
            else if (number <= 0xFFFF) size = 2;
            else if (number <= 0xFFFFFFFF) size = 4;
            else if (number <= 0xFFFFFFFFFFFFFFFF) size = 8;
            else {
                throw_error(isolate, "Invalid unsigned value");
                return false;
            }
            return true;
        }
        case 'f':
            size = 4;
            return true;
        case 'F':  // double
            size = 8;
            return true;
        case 's':
        case '8':
            size = contentVal->ToString()->Utf8Length();
            return true;
        case 'b':
            if (!Buffer::HasInstance(contentVal)) {
                throw_error(isolate, "Invalid buffer value");
                return false;
            }
            size = Buffer::Length(contentVal);
            return true;
        default:
            throw_error(isolate, "Invalid tag type");
            return false;
    }
}


static bool calc_ebml_size(Isolate *isolate, Handle<Value> val, size_t &size)
{
    size = 0;

    if (!val->IsArray()) {
        throw_error(isolate, "Expected an array of tags");
        return false;
    }

    auto tags = Handle<Array>::Cast(val);
    uint32_t length = tags->Length();
    for (uint32_t i = 0; i < length; i++) {
        auto tagVal = tags->Get(i);
        if (!tagVal->IsArray()) {
            throw_error(isolate, "Invalid tag");
            return false;
        }

        auto tag = Handle<Array>::Cast(tagVal);
        auto tagLength = tag->Length();
        if (tagLength < 3 || tagLength > 4) {
            throw_error(isolate, "Invalid tag");
            return false;
        }

        auto idVal = tag->Get(0);
        auto typeVal = tag->Get(1);
        auto contentVal = tag->Get(2);

        if (!Buffer::HasInstance(idVal)) {
            throw_error(isolate, "Invalid tag ID");
            return false;
        }

        auto idSize = Buffer::Length(idVal);
        if (idSize < 1 || idSize > 4) {
            throw_error(isolate, "Invalid tag ID");
            return false;
        }

        String::Utf8Value typeStr(typeVal);
        if (typeStr.length() != 1) {
            throw_error(isolate, "Invalid tag type");
            return false;
        }

        size_t contentSize;
        if (!calc_ebml_content_size(isolate, **typeStr, contentVal, contentSize))
            return false;

        size_t sizeSize = tag->Get(3)->IsTrue() ? 1 : calc_varint_size(contentSize);

        size += idSize + sizeSize + contentSize;
    }

    return true;
}


static bool write_ebml(Isolate *isolate, Handle<Value> val, uint8_t *dst)
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
        String::Utf8Value typeStr(typeVal);
        if (!calc_ebml_content_size(isolate, **typeStr, contentVal, contentSize))
            return false;

        if (tag->Get(3)->IsTrue()) {
            *dst = 0xFF;
            dst += 1;
        }
        else {
            dst += write_varint(contentSize, dst);
        }

        switch (**typeStr) {
            case 'm': {
                if (!write_ebml(isolate, contentVal, dst))
                    return false;
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

    return true;
}


void build_ebml(const FunctionCallbackInfo<Value>& args)
{
    auto *isolate = args.GetIsolate();
    HandleScope scope(isolate);

    if (args.Length() != 1)
        return throw_error(isolate, "Expected one argument");

    size_t size;
    if (!calc_ebml_size(isolate, args[0], size))
        return;

    Handle<Value> arg = Number::New(isolate, size);
    auto bufobj = fast_buffer_constructor.Get(isolate)->NewInstance(1, &arg);
    uint8_t *dst = (uint8_t *) Buffer::Data(bufobj);

    if (!write_ebml(isolate, args[0], dst))
        return;

    args.GetReturnValue().Set(bufobj);
}


}  // namespace p1stream
