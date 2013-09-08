#include "p1stream.h"

#include <signal.h>
#include <CoreFoundation/CoreFoundation.h>

static void terminate_handler(int sig);
static void notify_fd_callback(CFFileDescriptorRef fd, CFOptionFlags callBackTypes, void *info);
static void notify_exit_callback(CFFileDescriptorRef fd, CFOptionFlags callBackTypes, void *info);
static void create_video_clock(P1Context *ctx, P1Config *cfg);
static void create_audio_sources(P1Context *ctx, P1Config *cfg);
static bool create_audio_source(P1Config *cfg, P1ConfigSection *sect, void *data);
static void create_video_sources(P1Context *ctx, P1Config *cfg);
static bool create_video_source(P1Config *cfg, P1ConfigSection *sect, void *data);

static int sig_handler_pipe[2];


int main(int argc, const char * argv[])
{
    if (argc != 2) {
        printf("Usage: %s <config.plist>\n", argv[0]);
        return 2;
    }

    // Setup.
    P1Config *cfg = p1_plist_config_create_from_file(argv[1]);
    P1Context *ctx = p1_create(cfg, NULL);
    create_audio_sources(ctx, cfg);
    create_video_clock(ctx, cfg);
    create_video_sources(ctx, cfg);
    p1_config_free(cfg);

    // Shared stuff.
    CFFileDescriptorContext fdctx = {
        .version = 0,
        .info = ctx,
        .retain = NULL,
        .release = NULL,
        .copyDescription = NULL
    };
    CFFileDescriptorRef fd;
    CFRunLoopSourceRef src;

    // Listen for notifications.
    fd = CFFileDescriptorCreate(kCFAllocatorDefault, p1_fd(ctx), false, notify_fd_callback, &fdctx);
    CFFileDescriptorEnableCallBacks(fd, kCFFileDescriptorReadCallBack);
    src = CFFileDescriptorCreateRunLoopSource(kCFAllocatorDefault, fd, 0);
    CFRelease(fd);
    CFRunLoopAddSource(CFRunLoopGetMain(), src, kCFRunLoopDefaultMode);
    CFRelease(src);

    // Listen for signal handler notifications.
    int ret = pipe(sig_handler_pipe);
    assert(ret == 0);
    fd = CFFileDescriptorCreate(kCFAllocatorDefault, sig_handler_pipe[0], false, notify_exit_callback, &fdctx);
    CFFileDescriptorEnableCallBacks(fd, kCFFileDescriptorReadCallBack);
    src = CFFileDescriptorCreateRunLoopSource(kCFAllocatorDefault, fd, 0);
    CFRelease(fd);
    CFRunLoopAddSource(CFRunLoopGetMain(), src, kCFRunLoopDefaultMode);
    CFRelease(src);

    // Action!
    p1_start(ctx);

    // Signal handler.
    struct sigaction sa;
    sa.sa_handler = terminate_handler;
    sa.sa_mask = 0;
    sa.sa_flags = SA_NODEFER | SA_RESETHAND;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    CFRunLoopRun();
    return 0;
}

static void terminate_handler(int sig)
{
    char buf = 0;
    write(sig_handler_pipe[1], &buf, 1);
}

static void notify_fd_callback(CFFileDescriptorRef fd, CFOptionFlags callBackTypes, void *info)
{
    P1Context *ctx = (P1Context *) info;
    P1Object *ctxobj = (P1Object *) info;

    P1Notification n;
    p1_read(ctx, &n);

    if (n.object == ctxobj && n.type == P1_NTYPE_STATE_CHANGE && n.state_change.state == P1_STATE_IDLE) {
        p1_free(ctx, P1_FREE_EVERYTHING);
        CFRunLoopStop(CFRunLoopGetMain());
    }
    else {
        CFFileDescriptorEnableCallBacks(fd, kCFFileDescriptorReadCallBack);
    }
}

static void notify_exit_callback(CFFileDescriptorRef fd, CFOptionFlags callBackTypes, void *info)
{
    P1Context *ctx = (P1Context *) info;

    close(sig_handler_pipe[0]);
    close(sig_handler_pipe[1]);

    p1_stop(ctx, P1_STOP_ASYNC);
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

    ctx->video->clock = clock;
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

    P1AudioSource *asrc = factory(cfg, sect);
    assert(asrc != NULL);

    P1Source *src = (P1Source *) asrc;
    p1_list_before(&ctx->audio->sources, &src->link);

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

    P1VideoSource *vsrc = factory(cfg, sect);
    assert(vsrc != NULL);

    P1Source *src = (P1Source *) vsrc;
    p1_list_before(&ctx->video->sources, &src->link);

    return true;
}
