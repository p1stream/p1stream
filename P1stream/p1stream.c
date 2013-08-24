#include "p1stream_priv.h"

static void p1_progress_state(P1Context *ctx, P1Source *src);


P1Context *p1_create(P1Config *cfg, P1ConfigSection *sect)
{
    P1ContextFull *ctx = calloc(1, sizeof(P1ContextFull));

    mach_timebase_info(&ctx->timebase);

    P1ConfigSection *audio_sect = cfg->get_section(cfg, sect, "audio");
    p1_audio_init(ctx, cfg, audio_sect);

    P1ConfigSection *video_sect = cfg->get_section(cfg, sect, "video");
    p1_video_init(ctx, cfg, video_sect);

    P1ConfigSection *stream_sect = cfg->get_section(cfg, sect, "stream");
    p1_stream_init(ctx, cfg, stream_sect);

    return (P1Context *) ctx;
}

void p1_free(P1Context *ctx, P1FreeOptions options)
{
    // FIXME
}

void p1_start(P1Context *ctx)
{
    // FIXME

    P1VideoClock *vclock = ctx->clock;
    assert(vclock->state == P1StateIdle);

    vclock->ctx = ctx;
    vclock->start(vclock);
    assert(vclock->state == P1StateStarting || vclock->state == P1StateRunning);
}

void p1_stop(P1Context *ctx)
{
    // FIXME
}

void p1_clock_tick(P1VideoClock *vclock, int64_t time)
{
    P1Context *ctx = vclock->ctx;
    assert(vclock == ctx->clock);

    P1ListNode *head;
    P1ListNode *node;

    head = &ctx->video_sources;
    p1_list_iterate(head, node) {
        P1Source *src = (P1Source *) node;
        p1_progress_state(ctx, src);
    }

    head = &ctx->audio_sources;
    p1_list_iterate(head, node) {
        P1Source *src = (P1Source *) node;
        p1_progress_state(ctx, src);
    }

    p1_video_output(vclock, time);
}

static void p1_progress_state(P1Context *ctx, P1Source *src)
{
    if (src->target == P1TargetRunning) {
        if (src->state == P1StateIdle) {
            src->ctx = ctx;
            src->start(src);
            assert(src->state == P1StateStarting || src->state == P1StateRunning);
        }
    }
    else {
        if (src->state == P1StateRunning) {
            src->stop(src);
            assert(src->state == P1StateStopping || src->state == P1StateIdle);
        }
        if (src->target == P1TargetRemove && src->state == P1TargetIdle) {
            p1_list_remove(src);
            src->free(src);
        }
    }
}