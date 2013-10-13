#include "p1stream.h"

#include <signal.h>
#include <sysexits.h>
#import <Cocoa/Cocoa.h>

static void setup(const char *config_file);
static void terminate_handler(int sig);
static void notify_fd_callback(CFFileDescriptorRef fd, CFOptionFlags callBackTypes, void *info);
static void notify_exit_callback(CFFileDescriptorRef fd, CFOptionFlags callBackTypes, void *info);
static void create_audio_sources(P1Context *ctx, NSDictionary *dict);
static void create_video_clock(P1Context *ctx, NSDictionary *dict);
static void create_video_sources(P1Context *ctx, NSDictionary *dict);

static int sig_handler_pipe[2];

// FIXME: Replace property lists with something else cross platformy.


int main(int argc, const char * argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <config.plist>\n", argv[0]);
        return EX_USAGE;
    }

    @autoreleasepool {
        setup(argv[1]);
        CFRunLoopRun();
    }

    return EX_OK;
}

static void setup(const char *file) {
    NSString *ns_file = [NSString stringWithUTF8String:file];
    NSDictionary *dict = [NSDictionary dictionaryWithContentsOfFile:ns_file];
    if (!dict) {
        fprintf(stderr, "Failed to read '%s'.\n", file);
        exit(EX_DATAERR);
    }

    // Context.
    P1Config *cfg = p1_plist_config_create(dict);
    if (cfg == NULL)
        exit(EX_SOFTWARE);

    P1Context *ctx = p1_create();
    if (ctx == NULL) {
        p1_config_free(cfg);
        exit(EX_SOFTWARE);
    }

    p1_config(ctx, cfg);
    p1_config_free(cfg);

    // Objects.
    create_audio_sources(ctx, dict);
    create_video_clock(ctx, dict);
    create_video_sources(ctx, dict);

    // Shared file descriptor variables.
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

    if (n.object == ctxobj && n.state.current == P1_STATE_IDLE) {
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

static void create_audio_sources(P1Context *ctx, NSDictionary *dict)
{
    NSArray *sources_array = dict[@"audio-sources"];
    for (NSDictionary *source_dict in sources_array) {
        NSString *type = source_dict[@"type"];
        if (!type) {
            fprintf(stderr, "Missing audio source type.\n");
            exit(EX_DATAERR);
        }

        P1AudioSourceFactory *factory = NULL;
        if ([type isEqualToString:@"input"])
            factory = p1_input_audio_source_create;

        if (factory == NULL) {
            fprintf(stderr, "Invalid audio source type.\n");
            exit(EX_DATAERR);
        }

        P1Config *cfg = p1_plist_config_create(source_dict);
        if (cfg == NULL)
            exit(EX_SOFTWARE);

        P1AudioSource *source = factory(ctx);
        if (source == NULL) {
            p1_config_free(cfg);
            exit(EX_SOFTWARE);
        }

        p1_audio_source_config(source, cfg);
        p1_config_free(cfg);

        p1_list_before(&ctx->audio->sources, &((P1Source *) source)->link);
    }
}

static void create_video_clock(P1Context *ctx, NSDictionary *dict)
{
    NSDictionary *clock_dict = dict[@"video-clock"];
    if (!clock_dict) {
        fprintf(stderr, "Missing video clock configuration.\n");
        exit(EX_DATAERR);
    }

    NSString *type = clock_dict[@"type"];
    if (!type) {
        fprintf(stderr, "Missing video clock type.\n");
        exit(EX_DATAERR);
    }

    P1VideoClockFactory *factory = NULL;
    if ([type isEqualToString:@"display"])
        factory = p1_display_video_clock_create;

    if (factory == NULL) {
        fprintf(stderr, "Invalid video clock type.\n");
        exit(EX_DATAERR);
    }

    P1Config *cfg = p1_plist_config_create(dict);
    if (cfg == NULL)
        exit(EX_SOFTWARE);

    P1VideoClock *clock = factory(ctx);
    if (cfg == NULL) {
        p1_config_free(cfg);
        exit(EX_SOFTWARE);
    }

    p1_video_clock_config(clock, cfg);
    p1_config_free(cfg);

    ctx->video->clock = clock;
}

static void create_video_sources(P1Context *ctx, NSDictionary *dict)
{
    NSArray *sources_array = dict[@"video-sources"];
    for (NSDictionary *source_dict in sources_array) {
        NSString *type = source_dict[@"type"];
        if (!type) {
            fprintf(stderr, "Missing video source type.\n");
            exit(EX_DATAERR);
        }

        P1VideoSourceFactory *factory = NULL;
        if ([type isEqualToString:@"display"])
            factory = p1_display_video_source_create;
        else if ([type isEqualToString:@"capture"])
            factory = p1_capture_video_source_create;

        if (factory == NULL) {
            fprintf(stderr, "Invalid video source type.\n");
            exit(EX_DATAERR);
        }

        P1Config *cfg = p1_plist_config_create(source_dict);
        if (cfg == NULL)
            exit(EX_SOFTWARE);

        P1VideoSource *source = factory(ctx);
        if (source == NULL) {
            p1_config_free(cfg);
            exit(EX_SOFTWARE);
        }

        p1_video_source_config(source, cfg);
        p1_config_free(cfg);

        p1_list_before(&ctx->video->sources, &((P1Source *) source)->link);
    }
}
