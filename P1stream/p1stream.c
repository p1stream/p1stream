#include "p1stream_priv.h"


P1Context *p1_create(P1Config *cfg, P1ConfigSection *sect)
{
    P1Context *ctx = calloc(1, sizeof(P1Context));

    mach_timebase_info(&ctx->timebase);

    P1ConfigSection *audio_sect = cfg->get_section(cfg, sect, "audio");
    p1_audio_init(ctx, cfg, audio_sect);

    P1ConfigSection *video_sect = cfg->get_section(cfg, sect, "video");
    p1_video_init(ctx, cfg, video_sect);

    P1ConfigSection *stream_sect = cfg->get_section(cfg, sect, "stream");
    p1_stream_init(ctx, cfg, stream_sect);

    return ctx;
}
