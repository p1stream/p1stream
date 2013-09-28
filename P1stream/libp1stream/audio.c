#include "p1stream_priv.h"

#include <math.h>
#include <stdlib.h>
#include <memory.h>

// Hardcoded internal mixing buffer parameters.
static const int sample_rate = 44100;
static const int num_channels = 2;
// Buffers of one full second.
static const int buf_samples = num_channels * sample_rate;
// Minimum number of samples to gather before encoding.
static const int out_min_samples = buf_samples / 2;

static void p1_audio_kill_session(P1AudioFull *audiof);

static void p1_audio_write(P1AudioFull *ctx, P1AudioSource *asrc, float **in, size_t *samples);
static void p1_audio_resample(P1AudioFull *audiof);
static bool p1_audio_flush_out_buffer(P1AudioFull *audiof);

static size_t p1_audio_get_ready_mix_samples(P1AudioFull *audiof);
static void p1_audio_shift_mix_buffer(P1AudioFull *audiof, size_t samples);
static int64_t p1_audio_samples_to_time(P1ContextFull *ctx, size_t samples);


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
    P1Audio *audio = (P1Audio *) audiof;
    P1Object *audioobj = (P1Object *) audiof;
    P1ListNode *head;
    P1ListNode *node;

    p1_object_set_state(audioobj, P1_STATE_STARTING);

    audiof->mix = calloc(buf_samples, sizeof(float));
    if (audiof->mix == NULL) {
        p1_log(audioobj, P1_LOG_ERROR, "Failed to allocate audio mix buffer");
        goto fail_mix;
    }

    audiof->out = malloc(buf_samples * sizeof(int16_t));
    if (audiof->out == NULL) {
        p1_log(audioobj, P1_LOG_ERROR, "Failed to allocate audio output buffer");
        goto fail_out;
    }

    head = &audio->sources;
    if (head->next == head) {
        p1_log(audioobj, P1_LOG_ERROR, "No audio sources configured");
        goto fail_link;
    }

    bool found_master = false;
    p1_list_iterate(head, node) {
        P1Source *src = p1_list_get_container(node, P1Source, link);
        P1Object *obj = (P1Object *) src;
        P1AudioSource *asrc = (P1AudioSource *) src;

        if (asrc->master) {
            if (found_master) {
                p1_log(audioobj, P1_LOG_ERROR, "Multiple audio clock masters configured");
                goto fail_link;
            }
            found_master = true;
        }

        if (obj->state == P1_STATE_STARTING || obj->state == P1_STATE_RUNNING)
            p1_audio_link_source(asrc);
    }
    if (!found_master) {
        p1_log(audioobj, P1_LOG_WARNING, "No audio clock master, promoting first source");
        P1Source *src = p1_list_get_container(head->next, P1Source, link);
        P1AudioSource *asrc = (P1AudioSource *) src;
        asrc->master = true;
    }

    p1_object_set_state(audioobj, P1_STATE_RUNNING);

    return;

fail_link:
    free(audiof->out);

fail_out:
    free(audiof->mix);

fail_mix:
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
    // Note: We don't call unlink, because it's a no-op, currently.

    free(audiof->out);
    free(audiof->mix);
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
    P1Object *connobj = (P1Object *) conn;
    bool ret;

    p1_object_lock(audioobj);

    if (audioobj->state != P1_STATE_RUNNING)
        goto end;

    // Recalculate buffer start times.
    if (asrc->master) {
        audiof->mix_time = time - p1_audio_samples_to_time(ctxf, asrc->mix_pos);
        audiof->out_time = audiof->mix_time - p1_audio_samples_to_time(ctxf, audiof->out_pos);
    }

    do {
        // Write as many new samples to the mix buffer as we can.
        p1_audio_write(audiof, asrc, &in, &samples);

        // FIXME: preview

        // Streaming. The state test is a preliminary check. The state may change,
        // and the connection code does a final check itself, but checking here as
        // well saves us a bunch of processing.
        if (connobj->state == P1_STATE_RUNNING) {
            // Resample into the output buffer.
            p1_audio_resample(audiof);

            // Flush the output buffer.
            ret = p1_audio_flush_out_buffer(audiof);
        }
        else {
            // Ditch mixed samples.
            size_t samples = p1_audio_get_ready_mix_samples(audiof);
            p1_audio_shift_mix_buffer(audiof, samples);

            // Clear output buffer.
            audiof->out_pos = 0;

            // Continue if we can mix.
            ret = (samples > 0 && asrc->mix_pos < buf_samples);
        }
    } while (ret);

    if (samples)
        p1_log(audioobj, P1_LOG_WARNING, "Audio mix buffer full, dropped %zd samples!", samples);

end:
    p1_object_unlock(audioobj);
}

// Write as much as possible to the mix buffer.
static void p1_audio_write(P1AudioFull *audiof, P1AudioSource *asrc, float **in, size_t *samples)
{
    size_t to_write = buf_samples - asrc->mix_pos;
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

// Resample to the output buffer.
static void p1_audio_resample(P1AudioFull *audiof)
{
    // Determine the number of samples we can write.
    size_t samples = p1_audio_get_ready_mix_samples(audiof);
    size_t start = audiof->out_pos;
    size_t remaining = buf_samples - start;
    if (samples > remaining)
        samples = remaining;

    if (samples == 0)
        return;

    size_t end = start + samples;
    audiof->out_pos = end;

    // Write as 16-bit.
    float *mix = audiof->mix;
    int16_t *out = audiof->out;
    for (size_t i = start; i < end; i++) {
        float sample = mix[i];
        if (sample > +1.0) sample = +1.0;
        if (sample < -1.0) sample = -1.0;
        out[i] = (int16_t) (sample * INT16_MAX);
    }

    // Remove the samples.
    p1_audio_shift_mix_buffer(audiof, samples);
}

// Flush samples from the output buffer to the connection.
static bool p1_audio_flush_out_buffer(P1AudioFull *audiof)
{
    P1Audio *audio = (P1Audio *) audiof;
    P1Object *audioobj = (P1Object *) audio;
    P1Context *ctx = audioobj->ctx;
    P1ContextFull *ctxf = (P1ContextFull *) ctx;
    P1Connection *conn = ctx->conn;
    P1ConnectionFull *connf = (P1ConnectionFull *) conn;

    size_t out_available = audiof->out_pos;
    if (out_available == 0)
        return false;

    int16_t *out = audiof->out;
    size_t samples = p1_conn_stream_audio(connf, audiof->out_time, audiof->out, out_available);

    // Move remaining data up in the mix buffer.
    size_t out_remaining = out_available - samples;
    if (out_remaining)
        memmove(out, out + samples, out_remaining * sizeof(float));
    audiof->out_pos = out_remaining;

    // Recalculate out buffer start time.
    audiof->out_time += p1_audio_samples_to_time(ctxf, samples);

    return (samples != 0);
}


// Determine the number samples ready in the mix buffer.
static size_t p1_audio_get_ready_mix_samples(P1AudioFull *audiof)
{
    P1Audio *audio = (P1Audio *) audiof;
    P1ListNode *head;
    P1ListNode *node;

    size_t samples = buf_samples + 1;
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
    if (samples > buf_samples)
        return 0;
    else
        return samples;
}

// Shift samples off the mix buffer.
static void p1_audio_shift_mix_buffer(P1AudioFull *audiof, size_t samples)
{
    P1Audio *audio = (P1Audio *) audiof;
    P1Object *audioobj = (P1Object *) audio;
    P1Context *ctx = audioobj->ctx;
    P1ContextFull *ctxf = (P1ContextFull *) ctx;
    float *mix = audiof->mix;
    P1ListNode *head;
    P1ListNode *node;

    // Move remaining data up in the mix buffer.
    size_t mix_remaining = buf_samples - samples;
    if (mix_remaining)
        memmove(mix, mix + samples, mix_remaining * sizeof(float));
    memset(mix + mix_remaining, 0, samples * sizeof(float));

    // Adjust source positions.
    head = &audio->sources;
    p1_list_iterate(head, node) {
        P1Source *src = p1_list_get_container(node, P1Source, link);
        P1AudioSource *asrc = (P1AudioSource *) src;
        asrc->mix_pos -= samples;
    }

    // Recalculate mix buffer start time.
    audiof->mix_time += p1_audio_samples_to_time(ctxf, samples);
}

// Convert amount of samples to relative time they represent.
static int64_t p1_audio_samples_to_time(P1ContextFull *ctxf, size_t samples)
{
    int64_t nanosec = samples / num_channels * 1000000000 / sample_rate;
    return nanosec * ctxf->timebase_den / ctxf->timebase_num;
}
