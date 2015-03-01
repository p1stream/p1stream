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

// Helper functions.
static Local<Value> audio_frame_to_js(Isolate *isolate, audio_frame_data &frame, buffer_slicer &slicer);


void audio_mixer_full::init(const FunctionCallbackInfo<Value>& args)
{
    bool ok;
    AACENC_ERROR aac_err;
    AACENC_InfoStruct aac_info;
    isolate = args.GetIsolate();
    Handle<Value> val;

    if (args.Length() != 1 || !args[0]->IsObject()) {
        isolate->ThrowException(Exception::TypeError(
            String::NewFromUtf8(isolate, "Expected an object")));
        return;
    }
    auto params = args[0].As<Object>();

    val = params->Get(on_event_sym.Get(isolate));
    if (!val->IsFunction()) {
        isolate->ThrowException(Exception::TypeError(
            String::NewFromUtf8(isolate, "Expected an onEvent function")));
        return;
    }

    // Parameters checked, from here on we no longer throw exceptions.
    Wrap(args.This());
    Ref();
    args.GetReturnValue().Set(handle(isolate));

    callback.init(std::bind(&audio_mixer_full::emit_events, this));
    context.Reset(isolate, isolate->GetCurrentContext());
    on_event.Reset(isolate, val.As<Function>());

    aac_err = aacEncOpen(&enc, 0x01, 2);
    if (!(ok = (aac_err == AACENC_OK)))
        buffer.emitf(EV_LOG_ERROR, "aacEncOpen error 0x%x", aac_err);

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
                buffer.emitf(EV_LOG_ERROR, "aacEncoder_SetParam error 0x%x", aac_err);
                break;
            }
        }
    }

    if (ok) {
        aac_err = aacEncEncode(enc, NULL, NULL, NULL, NULL);
        if (!(ok = (aac_err == AACENC_OK)))
            buffer.emitf(EV_LOG_ERROR, "aacEncEncode error 0x%x", aac_err);
    }

    if (ok) {
        mix_buffer = new float[mix_samples];
        mix_time = system_time() - samples_to_time(mix_half_samples);

        aac_err = aacEncInfo(enc, &aac_info);
        if (!(ok = (aac_err == AACENC_OK)))
            buffer.emitf(EV_LOG_ERROR, "aacEncInfo error 0x%x", aac_err);
    }

    if (ok) {
        size_t claim = sizeof(audio_frame_data) + aac_info.confSize;
        auto *ev = buffer.emitd(EV_AUDIO_HEADERS, claim);
        if (!(ok = (ev != NULL))) {
            buffer.emitf(EV_LOG_ERROR, "buffer.emitd error");
        }
        else {
            auto &headers = *(audio_frame_data *) ev->data;
            headers.pts = 0;
            headers.size = aac_info.confSize;
            memcpy(headers.data, aac_info.confBuf, aac_info.confSize);
        }
    }

    if (ok) {
        thread.init(std::bind(&audio_mixer_full::loop, this));
        running = true;
    }
    else {
        buffer.emit(EV_FAILURE);
    }

    callback.send();
}

void audio_mixer_full::destroy()
{
    if (running) {
        thread.destroy();
        running = false;
    }

    clear_sources();

    if (mix_buffer != nullptr) {
        delete[] mix_buffer;
        mix_buffer = nullptr;
    }

    if (enc != NULL) {
        AACENC_ERROR aac_err = aacEncClose(&enc);
        enc = NULL;
        if (aac_err != AACENC_OK)
            fprintf(stderr, "aacEncClose error 0x%x\n", aac_err);
    }

    callback.destroy();
    emit_events();

    on_event.Reset();
    context.Reset();

    Unref();
}

lockable *audio_mixer_full::lock()
{
    return thread.lock();
}

void audio_mixer_full::clear_sources()
{
    for (auto &ctx : source_ctxes)
        ctx.source()->unlink_audio_source(ctx);
    source_ctxes.clear();
}

void audio_mixer_full::set_sources(const FunctionCallbackInfo<Value>& args)
{
    if (args.Length() != 1 || !args[0]->IsArray()) {
        isolate->ThrowException(Exception::TypeError(
            String::NewFromUtf8(isolate, "Expected an array")));
        return;
    }
    auto arr = args[0].As<Array>();
    uint32_t len = arr->Length();

    auto l_source_sym = source_sym.Get(isolate);
    auto l_volume_sym = volume_sym.Get(isolate);

    std::vector<audio_source_context_full> new_ctxes;
    for (uint32_t i = 0; i < len; i++) {
        auto val = arr->Get(i);
        if (!val->IsObject()) {
            isolate->ThrowException(Exception::TypeError(
                String::NewFromUtf8(isolate, "Expected only objects in the array")));
            return;
        }
        auto obj = val.As<Object>();

        auto source_obj = obj->Get(l_source_sym);
        if (!source_obj->IsObject()) {
            isolate->ThrowException(Exception::TypeError(
                String::NewFromUtf8(isolate, "Invalid or missing source")));
            return;
        }
        auto *source = ObjectWrap::Unwrap<audio_source>(source_obj.As<Object>());

        new_ctxes.emplace_back(this, source);
        auto &ctx = new_ctxes.back();

        ctx.volume = obj->Get(l_volume_sym)->NumberValue();
    }

    // Parameters checked, from here on we no longer throw exceptions.
    lock_handle lock(thread);

    if (!running)
        return;

    clear_sources();
    source_ctxes = new_ctxes;

    for (uint32_t i = 0; i < len; i++) {
        auto &ctx = source_ctxes[i];
        ctx.source()->link_audio_source(ctx);
    }
}

void audio_source_context::render_buffer(int64_t time, float *in, size_t samples)
{
    auto &m = *((audio_mixer_full *) mixer_);
    lock_handle lock(m.thread);

    if (!m.running)
        return;

    // Check the lower bound of the mix buffer, and determine the start
    // position we're going to write samples to.
    size_t mix_pos = 0;
    if (time < m.mix_time) {
        m.buffer.emitf(EV_LOG_WARN, "audio mixer underflow, dropping audio frames");

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
        m.buffer.emitf(EV_LOG_WARN, "audio mixer overflow, dropping audio frames");

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
                buffer.emitf(EV_LOG_ERROR, "aacEncEncode error 0x%x", err);
                out_args.numOutBytes = 0;
            }

            // Push output frame to the buffer.
            if (out_args.numOutBytes != 0) {
                size_t claim = sizeof(audio_frame_data) + out_args.numOutBytes;
                auto *ev = buffer.emitd(EV_AUDIO_FRAME, claim);
                if (ev != NULL) {
                    auto &frame = *(audio_frame_data *) ev->data;
                    frame.pts = mixp_time;
                    frame.size = out_args.numOutBytes;
                    memcpy(frame.data, outbuf, out_args.numOutBytes);
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

            // Signal main thread.
            callback.send();
        }
    }
}

void audio_mixer_full::emit_events()
{
    event_buffer_copy *copy;
    {
        lock_handle lock(*this);
        copy = buffer.copy_out();
    }
    if (copy == NULL)
        return;

    HandleScope handle_scope(isolate);
    Context::Scope context_scope(Local<Context>::New(isolate, context));

    copy->callback(
        isolate, handle(isolate), Local<Function>::New(isolate, on_event),
        [](Isolate *isolate, event &ev, buffer_slicer &slicer) -> Local<Value> {
            switch (ev.id) {
                case EV_AUDIO_HEADERS:
                case EV_AUDIO_FRAME:
                    return audio_frame_to_js(isolate, *(audio_frame_data *) ev.data, slicer);
                default:
                    return Undefined(isolate);
            }
        }
    );
}

static Local<Value> audio_frame_to_js(Isolate *isolate, audio_frame_data &frame, buffer_slicer &slicer)
{
    auto obj = Object::New(isolate);
    obj->Set(pts_sym.Get(isolate), Number::New(isolate, frame.pts));
    obj->Set(buf_sym.Get(isolate), slicer.slice((char *) frame.data, frame.size));
    return obj;
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
