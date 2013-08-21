#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>

#include "p1stream_priv.h"


// Fixed internal mixing buffer parameters.
static const int sample_rate = 44100;
static const int sample_size = 2;
static const int num_channels = 2;
// Hardcoded bitrate.
static const int bit_rate = 128 * 1024;
// Size of one frame.
static const int frame_size = num_channels * sample_size;
// Mix buffer buffer of one full second.
static const int mix_size = frame_size * sample_rate;
// Minimum output buffer size per FDK AAC requirements.
static const int out_min_size = 6144 / 8 * num_channels;
// Complete output buffer size, also one full second.
static const int out_size = out_min_size * 64;

static int p1_audio_write(P1Context *ctx, void **in, int *in_len);
static int p1_audio_read(P1Context *ctx, int num);
static int64_t p1_audio_bytes_to_mach_time(P1Context *ctx, int bytes);


void p1_audio_init(P1Context *ctx, P1Config *cfg, P1ConfigSection *sect)
{
    AACENC_ERROR err;

    ctx->mix = calloc(1, mix_size);
    ctx->out = malloc(out_size);

    err = aacEncOpen(&ctx->aac, 0x01, 2);
    assert(err == AACENC_OK);

    err = aacEncoder_SetParam(ctx->aac, AACENC_AOT, AOT_AAC_LC);
    assert(err == AACENC_OK);
    err = aacEncoder_SetParam(ctx->aac, AACENC_SAMPLERATE, sample_rate);
    assert(err == AACENC_OK);
    err = aacEncoder_SetParam(ctx->aac, AACENC_CHANNELMODE, MODE_2);
    assert(err == AACENC_OK);
    err = aacEncoder_SetParam(ctx->aac, AACENC_BITRATE, bit_rate);
    assert(err == AACENC_OK);
    err = aacEncoder_SetParam(ctx->aac, AACENC_TRANSMUX, TT_MP4_RAW);
    assert(err == AACENC_OK);

    err = aacEncEncode(ctx->aac, NULL, NULL, NULL, NULL);
    assert(err == AACENC_OK);

    mach_timebase_info(&ctx->timebase);
}

void p1_audio_add_source(P1Context *ctx, P1AudioSource *src)
{
    assert(ctx->audio_src == NULL);
    ctx->audio_src = src;
    src->ctx = ctx;
}

void p1_audio_mix(P1AudioSource *src, int64_t time, void *in, int in_len)
{
    P1Context *ctx = src->ctx;
    assert(src == ctx->audio_src);

    if (!ctx->sent_audio_config) {
        ctx->sent_audio_config = true;
        p1_stream_audio_config(ctx);
    }

    // Calculate time for the start of the mix buffer.
    ctx->time = time - p1_audio_bytes_to_mach_time(ctx, ctx->mix_len);

    int out_size;
    do {
        // Write to the mix buffer.
        p1_audio_write(ctx, &in, &in_len);

        // Read, encode and stream from the mix buffer.
        out_size = p1_audio_read(ctx, ctx->mix_len);
        if (out_size) {
            p1_stream_audio(ctx, time, ctx->out, out_size);

            // Recalculate mix buffer start time.
            ctx->time += p1_audio_bytes_to_mach_time(ctx, out_size);
        }
    } while (out_size);

    if (in_len)
        printf("Audio mix buffer underrun, dropped %d bytes!", in_len);
}

// Write as much as possible to the mix buffer.
static int p1_audio_write(P1Context *ctx, void **in, int *in_len)
{
    int bytes = mix_size - ctx->mix_len;
    if (*in_len < bytes)
        bytes = *in_len;

    if (bytes != 0) {
        // FIXME: Mixing code goes here!
        memcpy(ctx->mix + ctx->mix_len, *in, bytes);

        *in += bytes;
        *in_len -= bytes;
        ctx->mix_len += bytes;
    }

    return bytes;
}

// Read as much as possible from the ring buffer.
static int p1_audio_read(P1Context *ctx, int bytes)
{
    // Prepare encoder arguments.
    INT el_sizes[] = { sample_size };

    void *enc_bufs[] = { ctx->mix };
    INT enc_identifiers[] = { IN_AUDIO_DATA };
    INT enc_sizes[] = { bytes };
    AACENC_BufDesc in_desc = {
        .numBufs = 1,
        .bufs = enc_bufs,
        .bufferIdentifiers = enc_identifiers,
        .bufSizes = enc_sizes,
        .bufElSizes = el_sizes
    };

    void *out_bufs[] = { ctx->out };
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
        .numInSamples = bytes / sample_size,
        .numAncBytes = 0
    };

    // Encode as much as we can; FDK AAC gives us small batches.
    AACENC_ERROR err;
    AACENC_OutArgs out_args = { .numInSamples = 1 };
    while (in_args.numInSamples && out_args.numInSamples && out_desc.bufSizes[0] > out_min_size) {
        err = aacEncEncode(ctx->aac, &in_desc, &out_desc, &in_args, &out_args);
        assert(err == AACENC_OK);

        size_t in_processed = out_args.numInSamples * sample_size;
        in_desc.bufs[0] += in_processed;
        in_desc.bufSizes[0] -= in_processed;

        size_t out_bytes = out_args.numOutBytes;
        out_desc.bufs[0] += out_bytes;
        out_desc.bufSizes[0] -= out_bytes;

        in_args.numInSamples -= out_args.numInSamples;
    }

    // Move remaining data up in the mix buffer.
    int mix_read = (int) (in_desc.bufs[0] - ctx->mix);
    if (mix_read) {
        int mix_remaining = ctx->mix_len - mix_read;
        if (mix_remaining)
            memmove(ctx->mix, in_desc.bufs[0], mix_remaining);
        memset(ctx->mix + mix_remaining, 0, mix_read);
        ctx->mix_len = mix_remaining;
    }

    return mix_read;
}

static int64_t p1_audio_bytes_to_mach_time(P1Context *ctx, int bytes)
{
    int samples = bytes / frame_size;
    int64_t nanosec = samples * 1000000000 / sample_rate;
    return nanosec * ctx->timebase.denom / ctx->timebase.numer;
}
