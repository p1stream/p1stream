#include "p1stream_priv.h"


P1Context *p1_create(P1Config *cfg)
{
    P1Context *ctx = calloc(1, sizeof(P1Context));

    mach_timebase_info(&ctx->timebase);

    p1_audio_init(ctx, cfg);
    p1_video_init(ctx, cfg);
    p1_stream_init(ctx, cfg);

    return ctx;
}
