#include "p1stream_priv.h"

#include <math.h>
#include <stdlib.h>
#include <memory.h>

// Fixed internal mixing buffer parameters.
static const int sample_rate = 44100;
static const int num_channels = 2;
// Hardcoded bitrate.
static const int bit_rate = 128 * 1024;
// Mix buffer buffer of one full second.
static const int mix_samples = num_channels * sample_rate;
// Minimum number of samples to gather before encoding.
static const int out_min_samples = mix_samples / 2;
// Minimum output buffer size per FDK AAC requirements.
static const int out_min_size = 6144 / 8 * num_channels;
// Complete output buffer size, also one full second.
static const int out_size = out_min_size * 64;

static void p1_audio_kill_session(P1AudioFull *audiof);
static void p1_audio_write(P1AudioFull *ctx, P1AudioSource *asrc, float **in, size_t *samples);
static ssize_t p1_audio_read(P1AudioFull *ctx);
static int64_t p1_audio_samples_to_mach_time(P1ContextFull *ctx, size_t samples);


bool p1_audio_init(P1AudioFull *audiof, P1Config *cfg, P1ConfigSection *sect)
{
    P1Audio *audio = (P1Audio *) audiof;
    P1Object *audioobj = (P1Object *) audiof;

    if (!p1_object_init(audioobj, P1_OTYPE_AUDIO))
        return false;

    p1_list_init(&audio->sources);

    return true;
}

void p1_audio_start(P1AudioFull *audiof)
{
    P1Object *audioobj = (P1Object *) audiof;
    AACENC_ERROR err = AACENC_OK;

    p1_object_set_state(audioobj, P1_STATE_STARTING);

    audiof->mix = calloc(mix_samples, sizeof(float));
    if (audiof->mix == NULL) goto fail_mix_buf;
    audiof->enc_in = malloc(mix_samples * sizeof(INT_PCM));
    if (audiof->enc_in == NULL) goto fail_enc_buf;
    audiof->out = malloc(out_size);
    if (audiof->out == NULL) goto fail_out_buf;

    err = aacEncOpen(&audiof->aac, 0x01, 2);
    if (err != AACENC_OK) goto fail_enc;

    err = aacEncoder_SetParam(audiof->aac, AACENC_AOT, AOT_AAC_LC);
    if (err != AACENC_OK) goto fail_setup;
    err = aacEncoder_SetParam(audiof->aac, AACENC_SAMPLERATE, sample_rate);
    if (err != AACENC_OK) goto fail_setup;
    err = aacEncoder_SetParam(audiof->aac, AACENC_CHANNELMODE, MODE_2);
    if (err != AACENC_OK) goto fail_setup;
    err = aacEncoder_SetParam(audiof->aac, AACENC_BITRATE, bit_rate);
    if (err != AACENC_OK) goto fail_setup;
    err = aacEncoder_SetParam(audiof->aac, AACENC_TRANSMUX, TT_MP4_RAW);
    if (err != AACENC_OK) goto fail_setup;

    err = aacEncEncode(audiof->aac, NULL, NULL, NULL, NULL);
    if (err != AACENC_OK) goto fail_setup;

    p1_object_set_state(audioobj, P1_STATE_RUNNING);

    return;

fail_setup:
    aacEncClose(&audiof->aac);

fail_enc:
    free(audiof->out);

fail_out_buf:
    free(audiof->enc_in);

fail_enc_buf:
    free(audiof->mix);

fail_mix_buf:
    if (err != AACENC_OK)
        p1_log(audioobj, P1_LOG_ERROR, "Failed to start audio mixer: FDK AAC error %d", err);
    else
        p1_log(audioobj, P1_LOG_ERROR, "Failed to start audio mixer: Buffer allocation failed");
    p1_object_set_state(audioobj, P1_STATE_HALTED);
}

void p1_audio_stop(P1AudioFull *audiof)
{
    P1Object *audioobj = (P1Object *) audiof;

    p1_object_set_state(audioobj, P1_STATE_STOPPING);
    p1_audio_kill_session(audiof);
    p1_object_set_state(audioobj, P1_STATE_IDLE);
}

static void p1_audio_kill_session(P1AudioFull *audiof)
{
    P1Object *audioobj = (P1Object *) audiof;
    AACENC_ERROR err;

    err = aacEncClose(&audiof->aac);
    if (err != AACENC_OK)
        p1_log(audioobj, P1_LOG_ERROR, "Failed to stop audio mixer: FDK AAC error %d", err);

    free(audiof->mix);
    free(audiof->enc_in);
    free(audiof->out);
}

bool p1_audio_source_init(P1AudioSource *asrc, P1Config *cfg, P1ConfigSection *sect)
{
    P1Object *obj = (P1Object *) asrc;

    if (!p1_object_init(obj, P1_OTYPE_AUDIO_SOURCE))
        return false;

    if (!cfg->get_float(cfg, sect, "volume", &asrc->volume))
        asrc->volume = 1.0;
    if (!cfg->get_bool(cfg, sect, "master", &asrc->master))
        asrc->master = false;

    return true;
}

void p1_audio_source_buffer(P1AudioSource *asrc, int64_t time, float *in, size_t samples)
{
    P1Object *obj = (P1Object *) asrc;
    P1Context *ctx = obj->ctx;
    P1ContextFull *ctxf = (P1ContextFull *) ctx;
    P1Audio *audio = ctx->audio;
    P1AudioFull *audiof = (P1AudioFull *) audio;
    P1Object *audioobj = (P1Object *) audio;
    P1Connection *conn = ctx->conn;
    P1ConnectionFull *connf = (P1ConnectionFull *) conn;

    p1_object_lock(audioobj);

    if (audioobj->state != P1_STATE_RUNNING)
        goto end;

    if (!audiof->sent_config) {
        audiof->sent_config = true;
        p1_conn_audio_config(connf);
    }

    // Recalculate time for the start of the mix buffer.
    if (asrc->master)
        audiof->time = time - p1_audio_samples_to_mach_time(ctxf, asrc->mix_pos);

    size_t out_size;
    do {
        // Write to the mix buffer.
        p1_audio_write(audiof, asrc, &in, &samples);

        // Read, encode and stream from the mix buffer.
        time = audiof->time;
        out_size = p1_audio_read(audiof);
        if (out_size > 0)
            p1_conn_audio(connf, time, audiof->out, out_size);
    } while (out_size > 0);

    if (samples)
        p1_log(audioobj, P1_LOG_WARNING, "Audio mix buffer full, dropped %zd samples!", samples);

end:
    p1_object_unlock(audioobj);
}

// Write as much as possible to the mix buffer.
static void p1_audio_write(P1AudioFull *audiof, P1AudioSource *asrc, float **in, size_t *samples)
{
    size_t to_write = mix_samples - asrc->mix_pos;
    if (*samples < to_write)
        to_write = *samples;

    if (to_write != 0) {
        // Mix samples.
        // FIXME: Maybe synchronize based on timestamps?
        float *p_in = *in;
        float *p_mix = audiof->mix + asrc->mix_pos;
        for (size_t i = 0; i < to_write; i++)
            *(p_mix++) += *(p_in++) * asrc->volume;

        // Progress positions.
        *in = p_in;
        *samples -= to_write;
        asrc->mix_pos += to_write;
    }
}

// Read as much as possible from the ring buffer.
static ssize_t p1_audio_read(P1AudioFull *audiof)
{
    P1Audio *audio = (P1Audio *) audiof;
    P1Object *audioobj = (P1Object *) audio;
    P1Context *ctx = audioobj->ctx;
    P1ContextFull *ctxf = (P1ContextFull *) ctx;
    P1ListNode *head;
    P1ListNode *node;

    // See how much data is ready.
    size_t samples = mix_samples + 1;
    head = &audio->sources;
    p1_list_iterate(head, node) {
        P1Source *src = p1_list_get_container(node, P1Source, link);
        P1Object *obj = (P1Object *) src;
        P1AudioSource *asrc = (P1AudioSource *) src;

        if (obj->state == P1_STATE_RUNNING) {
            if (asrc->mix_pos < samples)
                samples = asrc->mix_pos;
        }
    }
    if (samples > mix_samples || samples < out_min_samples)
        return 0;

    // Convert to 16-bit.
    // FIXME: this is wasteful, because we potentially do this multiple times
    // for the same samples. Predict reads using aac->nSamplesToRead.
    float *mix = audiof->mix;
    INT_PCM *enc_in = audiof->enc_in;
    for (size_t i = 0; i < samples; i++) {
        float sample = mix[i];
        if (sample > +1.0) sample = +1.0;
        if (sample < -1.0) sample = -1.0;
        enc_in[i] = (INT_PCM) (sample * SAMPLE_MAX);
    }

    // Prepare encoder arguments.
    INT el_sizes[] = { sizeof(INT_PCM) };

    void *enc_bufs[] = { enc_in };
    INT enc_identifiers[] = { IN_AUDIO_DATA };
    INT enc_sizes[] = { (INT) (samples * sizeof(INT_PCM)) };
    AACENC_BufDesc in_desc = {
        .numBufs = 1,
        .bufs = enc_bufs,
        .bufferIdentifiers = enc_identifiers,
        .bufSizes = enc_sizes,
        .bufElSizes = el_sizes
    };

    void *out_bufs[] = { audiof->out };
    INT out_identifiers[] = { OUT_BITSTREAM_DATA };
    INT out_sizes[] = { out_size };
    AACENC_BufDesc out_desc = {
        .numBufs = 1,
        .bufs = out_bufs,
        .bufferIdentifiers = out_identifiers,
        .bufSizes = out_sizes,
        .bufElSizes = el_sizes
    };

    AACENC_InArgs in_args = {
        .numInSamples = (INT) samples,
        .numAncBytes = 0
    };

    // Encode as much as we can; FDK AAC gives us small batches.
    AACENC_ERROR err;
    AACENC_OutArgs out_args = { .numInSamples = 1 };
    while (in_args.numInSamples && out_args.numInSamples && out_desc.bufSizes[0] > out_min_size) {
        err = aacEncEncode(audiof->aac, &in_desc, &out_desc, &in_args, &out_args);
        if (err != AACENC_OK) {
            p1_log(audioobj, P1_LOG_ERROR, "Audio encode failure: FDK AAC error %d", err);
            p1_object_set_state(audioobj, P1_STATE_HALTING);
            p1_audio_kill_session(audiof);
            p1_object_set_state(audioobj, P1_STATE_HALTED);
            return -1;
        }

        size_t in_processed = out_args.numInSamples * sizeof(INT_PCM);
        in_desc.bufs[0] += in_processed;
        in_desc.bufSizes[0] -= in_processed;

        size_t out_bytes = out_args.numOutBytes;
        out_desc.bufs[0] += out_bytes;
        out_desc.bufSizes[0] -= out_bytes;

        in_args.numInSamples -= out_args.numInSamples;
    }

    // Move remaining data up in the mix buffer.
    size_t mix_read = (INT_PCM *) in_desc.bufs[0] - enc_in;
    if (mix_read) {
        size_t mix_remaining = samples - mix_read;
        if (mix_remaining)
            memmove(mix, in_desc.bufs[0], mix_remaining * sizeof(float));
        memset(mix + mix_remaining, 0, mix_read * sizeof(float));

        // Adjust source positions.
        p1_list_iterate(head, node) {
            P1Source *src = p1_list_get_container(node, P1Source, link);
            P1AudioSource *asrc = (P1AudioSource *) src;
            asrc->mix_pos -= mix_read;
        }

        // Recalculate mix buffer start time.
        audiof->time += p1_audio_samples_to_mach_time(ctxf, mix_read);
    }

    return out_desc.bufs[0] - audiof->out;
}

static int64_t p1_audio_samples_to_mach_time(P1ContextFull *ctxf, size_t samples)
{
    int64_t nanosec = samples / num_channels * 1000000000 / sample_rate;
    return nanosec * ctxf->timebase.denom / ctxf->timebase.numer;
}