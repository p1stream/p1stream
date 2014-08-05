#include "p1stream_priv.h"

#include <vector>
#include <utility>
#include <string.h>
#include <node_buffer.h>

namespace p1stream {

// Hardcoded format parameters.
static const int sample_rate = 44100;
static const int num_channels = 2;

// We have a mix buffer of one second, but 'now' is roughly the center. This
// allows sources to write to the buffer going back slightly, but also
// forward. The latter happens because the mix thread only processes at an
// interval, and the source may have lower latency.
static const int mix_samples = num_channels * sample_rate;
static const int mix_half_samples = mix_samples / 2;
static const int mix_interval = 300000000;  // 300ms

// Hardcoded encoder paramters.
static const int enc_bit_rate = 128 * 1024;
static const int enc_frame_size = 1024;
static const int enc_frame_samples = enc_frame_size * num_channels;
static const int enc_out_bytes = 6144 / 8 * num_channels;

// Output buffer size.
static const int buffer_size = enc_out_bytes * 128;


void audio_mixer_full::init(const FunctionCallbackInfo<Value>& args)
{
    bool ok;
    AACENC_ERROR aac_err;
    AACENC_InfoStruct aac_info;

    Handle<Object> params;
    Handle<Value> val;

    Wrap(args.This());
    args.GetReturnValue().Set(handle());
    isolate = args.GetIsolate();
    context.Reset(isolate, isolate->GetCurrentContext());

    if (!(ok = (args.Length() == 1)))
        strcpy(last_error, "Expected one argument");
    else if (!(ok = args[0]->IsObject()))
        strcpy(last_error, "Expected an object");
    else
        params = args[0].As<Object>();

    if (ok) {
        val = params->Get(on_data_sym.Get(isolate));
        if (!(ok = val->IsFunction()))
            strcpy(last_error, "Invalid or missing onData handler");
    }

    if (ok) {
        on_data.Reset(isolate, val.As<Function>());

        val = params->Get(on_error_sym.Get(isolate));
        if (!(ok = val->IsFunction()))
            strcpy(last_error, "Invalid or missing onError handler");
    }

    if (ok) {
        on_error.Reset(isolate, val.As<Function>());

        aac_err = aacEncOpen(&enc, 0x01, 2);
        if (!(ok = (aac_err == AACENC_OK)))
            sprintf(last_error, "aacEncOpen error %d", aac_err);
    }

    if (ok) {
        std::vector<std::pair<AACENC_PARAM, UINT>> params = {
            { AACENC_AOT, AOT_AAC_LC },
            { AACENC_SAMPLERATE, sample_rate },
            { AACENC_CHANNELMODE, MODE_2 },
            { AACENC_BITRATE, enc_bit_rate },
            { AACENC_TRANSMUX, TT_MP4_RAW },
            { AACENC_GRANULE_LENGTH, enc_frame_size }
        };
        for (auto &param : params) {
            aac_err = aacEncoder_SetParam(enc, param.first, param.second);
            if (!(ok = (aac_err == AACENC_OK))) {
                sprintf(last_error, "aacEncoder_SetParam error %d", aac_err);
                break;
            }
        }
    }

    if (ok) {
        aac_err = aacEncEncode(enc, NULL, NULL, NULL, NULL);
        if (!(ok = (aac_err == AACENC_OK)))
            sprintf(last_error, "aacEncEncode error %d", aac_err);
    }

    if (ok) {
        mix_buffer = new float[mix_samples];
        mix_time = system_time() - samples_to_time(mix_half_samples);

        buffer = new uint8_t[buffer_size];
        buffer_pos = buffer;

        aac_err = aacEncInfo(enc, &aac_info);
        if (!(ok = (aac_err == AACENC_OK)))
            sprintf(last_error, "aacEncInfo error %d", aac_err);
    }

    if (ok) {
        auto *headers = (audio_mixer_frame *) buffer_pos;
        buffer_pos += sizeof(audio_mixer_frame) + aac_info.confSize;

        headers->pts = 0;
        headers->size = aac_info.confSize;
        memcpy(headers->data, aac_info.confBuf, aac_info.confSize);
    }

    if (ok) {
        callback.init(std::bind(&audio_mixer_full::emit_last, this));
        thread.init(std::bind(&audio_mixer_full::loop, this));

        running = true;

        Ref();
    }
    else {
        destroy(false);
        isolate->ThrowException(pop_last_error());
    }
}

void audio_mixer_full::destroy(bool unref)
{
    if (running) {
        thread.destroy();
        running = false;
    }

    clear_sources();

    callback.destroy();

    if (mix_buffer != nullptr) {
        delete[] mix_buffer;
        mix_buffer = nullptr;
    }

    if (buffer != nullptr) {
        delete[] buffer;
        buffer = nullptr;
    }

    if (enc != NULL) {
        AACENC_ERROR aac_err = aacEncClose(&enc);
        enc = NULL;
        if (aac_err != AACENC_OK)
            fprintf(stderr, "aacEncClose error %d\n", aac_err);
    }

    on_data.Reset();
    on_error.Reset();
    context.Reset();

    if (unref)
        Unref();
}

lockable *audio_mixer_full::lock()
{
    return thread.lock();
}

Handle<Value> audio_mixer_full::pop_last_error()
{
    Handle<Value> ret;
    if (last_error[0] != '\0') {
        ret = Exception::Error(String::NewFromUtf8(isolate, last_error));
        last_error[0] = '\0';
    }
    return ret;
}

void audio_mixer_full::clear_sources()
{
    for (auto &ctx : source_ctxes)
        ctx.source()->unlink_audio_source(ctx);
    source_ctxes.clear();
}

void audio_mixer_full::set_sources(const FunctionCallbackInfo<Value>& args)
{
    if (args.Length() != 1)
        return throw_type_error("Expected one argument");

    if (!args[0]->IsArray())
        return throw_type_error("Expected an array");

    lock_handle lock(thread);

    clear_sources();

    bool ok = true;
    Handle<Value> val;

    auto arr = args[0].As<Array>();
    uint32_t len = arr->Length();

    auto l_source_sym = source_sym.Get(isolate);
    auto l_volume_sym = volume_sym.Get(isolate);

    for (uint32_t i = 0; i < len; i++) {
        auto val = arr->Get(i);
        if (!(ok = val->IsObject())) {
            throw_type_error("Expected only objects in the array");
            break;
        }

        auto obj = val.As<Object>();

        val = obj->Get(l_source_sym);
        if (!(ok = val->IsObject())) {
            throw_type_error("Invalid or missing source");
            break;

        }
        auto source_obj = val.As<Object>();
        auto *source = ObjectWrap::Unwrap<audio_source>(source_obj);

        source_ctxes.emplace_back(this, source);
        auto &ctx = source_ctxes.back();

        ctx.volume = obj->Get(l_volume_sym)->NumberValue();

        if (!(ok = ctx.source()->link_audio_source(ctx))) {
            source_ctxes.pop_back();
            break;
        }
    }

    if (!ok)
        clear_sources();
}

void audio_source_context::render_buffer(int64_t time, float *in, size_t samples)
{
    auto &m = *((audio_mixer_full *) mixer_);
    lock_handle lock(m.thread);

    // Check the lower bound of the mix buffer, and determine the start
    // position we're going to write samples to.
    size_t mix_pos = 0;
    if (time < m.mix_time) {
        // FIXME: Improve logging
        fprintf(stderr, "audio mixer underflow, dropping audio frames\n");

        size_t to_drop = m.time_to_samples(m.mix_time - time);
        if (to_drop >= samples)
            return;

        in += to_drop;
        samples -= to_drop;
    }
    else {
        mix_pos = m.time_to_samples(time - m.mix_time);
    }

    // Check the upperbound of the mix buffer, and determine the end
    // position we're going to write samples to.
    size_t mix_end = mix_pos + samples;
    if (mix_end > mix_samples) {
        // FIXME: Improve logging
        fprintf(stderr, "audio mixer overflow, dropping audio frames\n");

        size_t to_drop = mix_end - mix_samples;
        if (to_drop >= samples)
            return;

        samples -= to_drop;
    }

    // Mix samples into the buffer.
    float *p = m.mix_buffer + mix_pos;
    float v = ((audio_source_context_full *) this)->volume;
    while (samples--)
        *(p++) += *(in++) * v;
}

// The mixer thread loop.
void audio_mixer_full::loop()
{
    int64_t enc_frame_time = samples_to_time(enc_frame_samples);
    INT_PCM resbuf[enc_frame_samples];
    UCHAR outbuf[enc_out_bytes];

    void *in_desc_bufs[] = { resbuf };
    INT in_desc_ids[] = { IN_AUDIO_DATA };
    INT in_desc_sizes[] = { enc_frame_samples * sizeof(INT_PCM) };
    INT in_desc_el_sizes[] = { sizeof(INT_PCM) };
    AACENC_BufDesc in_desc = {
        .numBufs           = 1,
        .bufs              = in_desc_bufs,
        .bufferIdentifiers = in_desc_ids,
        .bufSizes          = in_desc_sizes,
        .bufElSizes        = in_desc_el_sizes
    };

    void *out_desc_bufs[] = { outbuf };
    INT out_desc_ids[] = { OUT_BITSTREAM_DATA };
    INT out_desc_sizes[] = { enc_out_bytes };
    INT out_desc_el_sizes[] = { sizeof(UCHAR) };
    AACENC_BufDesc out_desc = {
        .numBufs           = 1,
        .bufs              = out_desc_bufs,
        .bufferIdentifiers = out_desc_ids,
        .bufSizes          = out_desc_sizes,
        .bufElSizes        = out_desc_el_sizes
    };

    AACENC_InArgs in_args = {
        .numInSamples = enc_frame_samples,
        .numAncBytes  = 0
    };

    AACENC_OutArgs out_args;

    // Continue until the thread is stopped.
    while (!thread.wait(mix_interval)) {
        // Process mixed samples until the mix buffer is roughly recentered.
        // Take steps forward in encoder frame sized chunks.
        float *mixp = mix_buffer;
        size_t processed = 0;
        int64_t target_mix_time = system_time() - samples_to_time(mix_half_samples);
        int64_t mixp_time = mix_time;
        size_t available = buffer + buffer_size - buffer_pos;
        while (target_mix_time > mixp_time) {
            // Resample to signed integer.
            INT_PCM *resp = resbuf;
            int count = enc_frame_samples;
            while (count--) {
                float sample = *(mixp++);
                if (sample > +1.0) sample = +1.0;
                if (sample < -1.0) sample = -1.0;
                *(resp++) = (INT_PCM) (sample * SAMPLE_MAX);
            }

            // Encode the integer samples.
            AACENC_ERROR err = aacEncEncode(enc, &in_desc, &out_desc, &in_args, &out_args);
            if (err != AACENC_OK) {
                sprintf(last_error, "aacEncEncode error %d", err);
                out_args.numOutBytes = 0;
            }

            // Push output frame to the buffer.
            if (out_args.numOutBytes != 0) {
                size_t claim = sizeof(audio_mixer_frame) + out_args.numOutBytes;

                if (claim > available) {
                    // FIXME: Improve logging
                    fprintf(stderr, "audio mixer overflow, dropping audio frames\n");
                }
                else {
                    auto *frame = (audio_mixer_frame *) buffer_pos;
                    buffer_pos += claim;
                    available -= claim;

                    frame->pts = mixp_time;
                    frame->size = out_args.numOutBytes;
                    memcpy(frame->data, outbuf, out_args.numOutBytes);
                }
            }

            mixp_time += enc_frame_time;
            processed += enc_frame_samples;
        }

        // FIXME: Handle lag, where processed >= mix_samples

        if (processed > 0) {
            // Shift processed samples off the mix buffer.
            size_t mix_remaining = mix_samples - processed;
            if (mix_remaining)
                memmove(mix_buffer, mixp, mix_remaining * sizeof(float));
            memset(mix_buffer + mix_remaining, 0, processed * sizeof(float));

            // Adjust mix buffer time.
            mix_time = mixp_time;
        }

        // Signal main thread.
        if (processed > 0 || last_error[0] != '\0')
            callback.send();
    }
}

void audio_mixer_full::emit_last()
{
    HandleScope handle_scope(isolate);
    Context::Scope context_scope(Local<Context>::New(isolate, context));

    Handle<Value> err;
    uint8_t *copy = NULL;
    size_t size = 0;

    // With lock, extract a copy of buffer (or error).
    {
        lock_handle lock(*this);
        err = pop_last_error();
        if (err.IsEmpty()) {
            size = buffer_pos - buffer;
            if (size != 0) {
                copy = new uint8_t[size];
                memcpy(copy, buffer, size);
                buffer_pos = buffer;
            }
        }
    }

    if (!err.IsEmpty()) {
        auto l_on_error = Local<Function>::New(isolate, on_error);
        MakeCallback(isolate, handle(isolate), l_on_error, 1, &err);
    }
    if (size == 0)
        return;

    // Create meta structure from buffer.
    auto obj = Object::New(isolate);

    auto buf = Buffer::New(isolate, (char *) copy, size, free_callback, NULL);
    obj->Set(buf_sym.Get(isolate), buf);

    auto frames_arr = Array::New(isolate);
    obj->Set(frames_sym.Get(isolate), frames_arr);

    auto l_pts_sym = pts_sym.Get(isolate);
    auto l_start_sym = start_sym.Get(isolate);
    auto l_end_sym = end_sym.Get(isolate);

    auto *p = copy;
    auto *end = p + size;
    uint32_t i_frame = 0;
    while (p != end) {
        auto *frame = (audio_mixer_frame *) p;
        auto frame_obj = Object::New(isolate);
        frames_arr->Set(i_frame++, frame_obj);

        off_t start = frame->data - copy;
        off_t end = start + frame->size;

        frame_obj->Set(l_pts_sym, Number::New(isolate, frame->pts));
        frame_obj->Set(l_start_sym, Uint32::New(isolate, start));
        frame_obj->Set(l_end_sym, Uint32::New(isolate, end));

        p = copy + end;
    }

    auto l_on_data = Local<Function>::New(isolate, on_data);
    Handle<Value> arg = obj;
    MakeCallback(isolate, handle(isolate), l_on_data, 1, &arg);
}

// Convert amount of samples to relative time they represent.
size_t audio_mixer_full::time_to_samples(int64_t time)
{
    return time * sample_rate / 1000000000 * num_channels;
}

// Convert amount of samples to relative time they represent.
int64_t audio_mixer_full::samples_to_time(size_t samples)
{
    return samples / num_channels * 1000000000 / sample_rate;
}

void audio_mixer_full::free_callback(char *data, void *hint)
{
    auto *p = (uint8_t *) data;
    delete[] p;
}

void audio_mixer_full::init_prototype(Handle<FunctionTemplate> func)
{
    NODE_SET_PROTOTYPE_METHOD(func, "destroy", [](const FunctionCallbackInfo<Value>& args) {
        auto mixer = ObjectWrap::Unwrap<audio_mixer_full>(args.This());
        mixer->destroy();
    });
    NODE_SET_PROTOTYPE_METHOD(func, "setSources", [](const FunctionCallbackInfo<Value>& args) {
        auto mixer = ObjectWrap::Unwrap<audio_mixer_full>(args.This());
        mixer->set_sources(args);
    });
}


} // namespace p1stream