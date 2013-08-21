#include <stdio.h>
#include <CoreFoundation/CoreFoundation.h>

#include "p1stream.h"


static void create_video_clock(P1Context *ctx, P1Config *cfg);
static void create_audio_sources(P1Context *ctx, P1Config *cfg);
static bool create_audio_source(P1Config *cfg, P1ConfigSection *sect, void *data);
static void create_video_sources(P1Context *ctx, P1Config *cfg);
static bool create_video_source(P1Config *cfg, P1ConfigSection *sect, void *data);


int main(int argc, const char * argv[])
{
    if (argc != 2) {
        printf("Usage: %s <config.plist>\n", argv[0]);
        return 2;
    }

    P1Config *cfg = p1_conf_plist_from_file(argv[1]);
    P1Context *ctx = p1_create(cfg, NULL);

    create_audio_sources(ctx, cfg);
    create_video_clock(ctx, cfg);
    create_video_sources(ctx, cfg);

    CFRunLoopRun();

    return 0;
}

static void create_video_clock(P1Context *ctx, P1Config *cfg)
{
    bool b_ret;

    P1ConfigSection *sect = cfg->get_section(cfg, NULL, "video.clock");
    assert(sect != NULL);

    char type[64];
    b_ret = cfg->get_string(cfg, sect, "type", type, sizeof(type));
    assert(b_ret == true);

    P1VideoClockFactory *factory;
    if (strcmp(type, "display") == 0)
        factory = p1_display_video_clock_create;
    else
        abort();

    P1VideoClock *clock = factory(cfg, sect);
    assert(clock != NULL);

    p1_video_set_clock(ctx, clock);

    b_ret = clock->start(clock);
    assert(b_ret == true);
}

static void create_audio_sources(P1Context *ctx, P1Config *cfg)
{
    bool b_ret = cfg->each_section(cfg, NULL, "audio.sources", create_audio_source, ctx);
    assert(b_ret == true);
}

static bool create_audio_source(P1Config *cfg, P1ConfigSection *sect, void *data)
{
    P1Context *ctx = (P1Context *) data;
    bool b_ret;

    char type[64];
    b_ret = cfg->get_string(cfg, sect, "type", type, sizeof(type));
    assert(b_ret == true);

    P1AudioSourceFactory *factory;
    if (strcmp(type, "input") == 0)
        factory = p1_input_audio_source_create;
    else
        abort();

    P1AudioSource *src = factory(cfg, sect);
    assert(src != NULL);

    p1_audio_add_source(ctx, src);

    b_ret = src->start(src);
    assert(b_ret == true);

    return true;
}

static void create_video_sources(P1Context *ctx, P1Config *cfg)
{
    bool b_ret = cfg->each_section(cfg, NULL, "video.sources", create_video_source, ctx);
    assert(b_ret == true);
}

static bool create_video_source(P1Config *cfg, P1ConfigSection *sect, void *data)
{
    P1Context *ctx = (P1Context *) data;
    bool b_ret;

    char type[64];
    b_ret = cfg->get_string(cfg, sect, "type", type, sizeof(type));
    assert(b_ret == true);

    P1VideoSourceFactory *factory;
    if (strcmp(type, "display") == 0)
        factory = p1_display_video_source_create;
    else if (strcmp(type, "capture") == 0)
        factory = p1_capture_video_source_create;
    else
        abort();

    P1VideoSource *src = factory(cfg, sect);
    assert(src != NULL);

    p1_video_add_source(ctx, src);

    b_ret = src->start(src);
    assert(b_ret == true);

    return true;
}
