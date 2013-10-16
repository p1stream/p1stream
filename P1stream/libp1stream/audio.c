#include "p1stream_priv.h"

#include <math.h>
#include <stdlib.h>
#include <memory.h>
#include <sys/time.h>

// Hardcoded internal mixing buffer parameters.
static const int sample_rate = 44100;
static const int num_channels = 2;
// Buffers of two seconds.
static const int buf_samples = num_channels * sample_rate * 2;
static const int buf_center = buf_samples / 2;
// Interval in usec at which we process mixed samples.
static const int mix_interval = 300000;

static void *p1_audio_main(void *data);
static void p1_audio_resample(P1AudioFull *audiof, size_t samples);
static void p1_audio_shift_mix_buffer(P1AudioFull *audiof, size_t samples);
static void p1_audio_flush_out_buffer(P1AudioFull *audiof);

static size_t p1_audio_time_to_samples(P1ContextFull *ctxf, int64_t time);
static int64_t p1_audio_samples_to_time(P1ContextFull *ctx, size_t samples);


bool p1_audio_init(P1AudioFull *audiof, P1Context *ctx)
{
    P1Audio *audio = (P1Audio *) audiof;
    P1Object *audioobj = (P1Object *) audiof;
    int ret;

    if (!p1_object_init(audioobj, P1_OTYPE_AUDIO, ctx))
        goto fail_object;

    ret = pthread_cond_init(&audiof->cond, NULL);
    if (ret != 0) {
        p1_log(audioobj, P1_LOG_ERROR, "Failed to initialize condition variable: %s", strerror(ret));
        goto fail_cond;
    }

    p1_list_init(&audio->sources);

    return true;

fail_cond:
    p1_object_destroy(audioobj);

fail_object:
    return false;
}

void p1_audio_destroy(P1AudioFull *audiof)
{
    P1Object *audioobj = (P1Object *) audiof;
    int ret;

    ret = pthread_cond_destroy(&audiof->cond);
    if (ret != 0)
        p1_log(audioobj, P1_LOG_ERROR, "Failed to destroy condition variable: %s", strerror(ret));

    p1_object_destroy(audioobj);
}

void p1_audio_config(P1AudioFull *audiof, P1Config *cfg)
{
    P1Object *audioobj = (P1Object *) audiof;

    p1_object_reset_config_flags(audioobj);

    // FIXME

    p1_object_notify(audioobj);
}

void p1_audio_notify(P1AudioFull *audiof, P1Notification *n)
{
    P1Object *audioobj = (P1Object *) audiof;

    p1_object_reset_notify_flags(audioobj);
    p1_object_notify(audioobj);
}

void p1_audio_start(P1AudioFull *audiof)
{
    P1Object *audioobj = (P1Object *) audiof;

    int ret = pthread_create(&audiof->thread, NULL, p1_audio_main, audiof);
    if (ret != 0) {
        p1_log(audioobj, P1_LOG_ERROR, "Failed to start audio mixer thread: %s", strerror(ret));
        audioobj->state.current = P1_STATE_IDLE;
        audioobj->state.flags |= P1_FLAG_ERROR;
    }
    else {
        // Thread will continue start, and set state to running
        audioobj->state.current = P1_STATE_STARTING;
    }

    p1_object_notify(audioobj);
}

void p1_audio_stop(P1AudioFull *audiof)
{
    P1Object *audioobj = (P1Object *) audiof;
    int ret;

    audioobj->state.current = P1_STATE_STOPPING;
    p1_object_notify(audioobj);

    ret = pthread_cond_signal(&audiof->cond);
    if (ret != 0)
        p1_log(audioobj, P1_LOG_ERROR, "Failed to signal audio mixer thread: %s", strerror(ret));
}


bool p1_audio_source_init(P1AudioSource *asrc, P1Context *ctx)
{
    return p1_object_init((P1Object *) asrc, P1_OTYPE_AUDIO_SOURCE, ctx);
}

void p1_audio_source_config(P1AudioSource *asrc, P1Config *cfg)
{
    P1Plugin *pel = (P1Plugin *) asrc;
    P1Object *obj = (P1Object *) asrc;

    p1_object_reset_config_flags(obj);

    if (!cfg->get_float(cfg, "volume", &asrc->volume))
        asrc->volume = 1.0;

    if (pel->config != NULL)
        pel->config(pel, cfg);

    p1_object_notify(obj);
}

void p1_audio_source_notify(P1AudioSource *asrc, P1Notification *n)
{
    P1Plugin *pel = (P1Plugin *) asrc;
    P1Object *obj = (P1Object *) asrc;

    p1_object_reset_notify_flags(obj);

    if (pel->notify != NULL)
        pel->notify(pel, n);

    p1_object_notify(obj);
}

void p1_audio_source_buffer(P1AudioSource *asrc, int64_t time, float *in, size_t samples)
{
    P1Object *obj = (P1Object *) asrc;
    P1Context *ctx = obj->ctx;
    P1ContextFull *ctxf = (P1ContextFull *) ctx;
    P1Audio *audio = ctx->audio;
    P1AudioFull *audiof = (P1AudioFull *) audio;
    P1Object *audioobj = (P1Object *) audio;

    p1_object_lock(audioobj);

    if (audioobj->state.current != P1_STATE_RUNNING)
        goto end;

    size_t mix_time = audiof->mix_time;

    // Check the lower bound of the mix buffer, and determine the position
    // we're going to write these new samples at.
    size_t mix_pos = 0;
    if (time < mix_time) {
        p1_log(audioobj, P1_LOG_WARNING, "Audio source %p has too much latency!", asrc);
        size_t to_drop = p1_audio_time_to_samples(ctxf, mix_time - time);
        if (to_drop >= samples) {
            goto end;
        }
        else {
            in += to_drop;
            samples -= to_drop;
        }
    }
    else {
        mix_pos = p1_audio_time_to_samples(ctxf, time - mix_time);
    }

    // Check the upper bound of the mix buffer.
    size_t mix_end = mix_pos + samples;
    if (mix_end > buf_samples) {
        p1_log(audioobj, P1_LOG_WARNING, "Audio mixer is lagging!");
        size_t to_drop = mix_end - buf_samples;
        if (to_drop >= samples)
            goto end;
        else
            samples -= to_drop;
    }

    // Mix samples into the buffer.
    float *mix = audiof->mix + mix_pos;
    float volume = asrc->volume;
    while (samples--)
        *(mix++) += *(in++) * volume;

end:
    p1_object_unlock(audioobj);
}


// The main loop of the streaming thread.
static void *p1_audio_main(void *data)
{
    P1AudioFull *audiof = (P1AudioFull *) data;
    P1Object *audioobj = (P1Object *) data;
    P1Context *ctx = audioobj->ctx;
    P1ContextFull *ctxf = (P1ContextFull *) ctx;
    P1Connection *conn = ctx->conn;
    P1Object *connobj = (P1Object *) conn;
    int ret;

    p1_object_lock(audioobj);

    audiof->mix = calloc(buf_samples, sizeof(float));
    if (audiof->mix == NULL) {
        p1_log(audioobj, P1_LOG_ERROR, "Failed to allocate audio mix buffer");
        audioobj->state.flags |= P1_FLAG_ERROR;
        goto cleanup;
    }

    audiof->out = malloc(buf_samples * sizeof(int16_t));
    if (audiof->out == NULL) {
        p1_log(audioobj, P1_LOG_ERROR, "Failed to allocate audio output buffer");
        audioobj->state.flags |= P1_FLAG_ERROR;
        goto cleanup_mix;
    }

    audiof->mix_time = p1_get_time() - p1_audio_samples_to_time(ctxf, buf_center);
    audiof->out_pos = 0;
    audiof->out_time = audiof->mix_time;

    audioobj->state.current = P1_STATE_RUNNING;
    p1_object_notify(audioobj);

    do {
        // Get the current time.
        struct timeval delay_end;
        ret = gettimeofday(&delay_end, NULL);
        if (ret != 0) {
            p1_log(audioobj, P1_LOG_ERROR, "Failed to get time: %s", strerror(errno));
            audioobj->state.flags |= P1_FLAG_ERROR;
            goto cleanup_out;
        }

        // Set the delay end time.
        delay_end.tv_usec += mix_interval;
        while (delay_end.tv_usec >= 1000000) {
            delay_end.tv_usec -= 1000000;
            delay_end.tv_sec++;
        }

        // Need to get a timespec of that.
        struct timespec delay_end_spec;
        delay_end_spec.tv_sec = delay_end.tv_sec;
        delay_end_spec.tv_nsec = delay_end.tv_usec * 1000;

        // Wait. If the condition is triggered, we're stopping.
        ret = pthread_cond_timedwait(&audiof->cond, &audioobj->lock, &delay_end_spec);
        if (ret == 0) {
            break;
        }
        else if (ret != ETIMEDOUT) {
            p1_log(audioobj, P1_LOG_ERROR, "Failed to wait on condition: %s", strerror(ret));
            audioobj->state.flags |= P1_FLAG_ERROR;
            goto cleanup_out;
        }

        // Process mixed samples up to this point.
        int64_t mix_time = p1_get_time() - p1_audio_samples_to_time(ctxf, buf_center);
        size_t samples = p1_audio_time_to_samples(ctxf, mix_time - audiof->mix_time);
        if (samples > buf_samples) {
            p1_log(audioobj, P1_LOG_WARNING, "Audio mixer is skipping!");
            samples = buf_samples;
        }

        // FIXME: preview

        // Streaming. The state test is a preliminary check. The state may change,
        // and the connection code does a final check itself, but checking here as
        // well saves us a bunch of processing.
        if (connobj->state.current == P1_STATE_RUNNING) {
            // Resample into the output buffer.
            p1_audio_resample(audiof, samples);

            // Flush the output buffer.
            p1_audio_flush_out_buffer(audiof);
        }
        else {
            // Clear output buffer.
            audiof->out_pos = 0;
        }

        // Remove the old samples.
        p1_audio_shift_mix_buffer(audiof, samples);

        // Adjust buffer start times.
        audiof->mix_time = mix_time;
        audiof->out_time = mix_time - p1_audio_samples_to_time(ctxf, audiof->out_pos);
    } while (true);

cleanup_out:
    free(audiof->out);

cleanup_mix:
    free(audiof->mix);

cleanup:
    audioobj->state.current = P1_STATE_IDLE;
    p1_object_notify(audioobj);

    p1_object_unlock(audioobj);

    return NULL;
}

// Resample to the output buffer.
static void p1_audio_resample(P1AudioFull *audiof, size_t samples)
{
    P1Object *audioobj = (P1Object *) audiof;

    // Determine the number of samples we can write.
    size_t start = audiof->out_pos;
    size_t remaining = buf_samples - start;
    if (samples > remaining) {
        p1_log(audioobj, P1_LOG_ERROR, "Audio encoder didn't keep up? This should never happen!");
        samples = remaining;
    }

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
}

// Shift samples off the mix buffer.
static void p1_audio_shift_mix_buffer(P1AudioFull *audiof, size_t samples)
{
    float *mix = audiof->mix;

    // Move data up in the buffer, and clear the freed area.
    size_t mix_remaining = buf_samples - samples;
    if (mix_remaining)
        memmove(mix, mix + samples, mix_remaining * sizeof(float));
    memset(mix + mix_remaining, 0, samples * sizeof(float));
}

// Flush samples from the output buffer to the connection.
static void p1_audio_flush_out_buffer(P1AudioFull *audiof)
{
    P1Audio *audio = (P1Audio *) audiof;
    P1Object *audioobj = (P1Object *) audio;
    P1Context *ctx = audioobj->ctx;
    P1ContextFull *ctxf = (P1ContextFull *) ctx;
    P1Connection *conn = ctx->conn;
    P1ConnectionFull *connf = (P1ConnectionFull *) conn;

    // Send as many frames as we can.
    size_t out_available = audiof->out_pos;
    if (out_available == 0)
        return;

    int16_t *out = audiof->out;
    size_t out_time = audiof->out_time;

    size_t total_samples = 0;
    size_t samples;
    do {
        samples = p1_conn_stream_audio(connf, audiof->out_time, out, out_available);
        if (samples == 0)
            break;

        out += samples;
        out_available -= samples;
        out_time += p1_audio_samples_to_time(ctxf, samples);

        total_samples += samples;
    } while(out_available);

    // Move remaining data up in the out buffer.
    if (total_samples == 0)
        return;

    size_t out_remaining = audiof->out_pos - total_samples;
    if (out_remaining)
        memmove(out, out + total_samples, out_remaining * sizeof(int16_t));

    audiof->out_pos = out_remaining;
    audiof->out_time = out_time;
}


// Convert amount of samples to relative time they represent.
static size_t p1_audio_time_to_samples(P1ContextFull *ctxf, int64_t time)
{
    int64_t nanosec = time * ctxf->timebase_num / ctxf->timebase_den;
    return nanosec * sample_rate / 1000000000 * num_channels;
}

// Convert amount of samples to relative time they represent.
static int64_t p1_audio_samples_to_time(P1ContextFull *ctxf, size_t samples)
{
    int64_t nanosec = samples / num_channels * 1000000000 / sample_rate;
    return nanosec * ctxf->timebase_den / ctxf->timebase_num;
}
