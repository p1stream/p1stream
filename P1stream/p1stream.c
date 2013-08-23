#include "p1stream_priv.h"


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
    P1VideoClock *clock = ctx->clock;
    clock->ctx = ctx;
    clock->start(clock);

    P1ListNode *head = &ctx->audio_sources;
    P1ListNode *node = head->next;
    while (node != head) {
        P1AudioSource *src = (P1AudioSource *) node;

        if (src->ctx != ctx) {
            src->ctx = ctx;
            // FIXME: target states
            src->start(src);
        }

        node = node->next;
    }
}

void p1_stop(P1Context *ctx)
{
    // FIXME
}
