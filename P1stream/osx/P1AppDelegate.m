#import "P1AppDelegate.h"


@implementation P1AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    terminating = false;

    // Register config defaults
    [defaults registerDefaults:@{
        @"Context configuration": @{
            @"video": @{
                @"clock": @{
                    @"type": @"display"
                },
                @"sources": @[
                    @{
                        @"type": @"display"
                    }
                ]
            },
            @"audio": @{
                @"sources": @[
                    @{
                        @"type": @"input",
                        @"master": @TRUE
                    }
                ]
            },
            @"stream": @{}
        }
    }];

    // Create context and objects
    NSDictionary *config = [defaults dictionaryForKey:@"Context configuration"];

    P1Config *p1Config = p1_plist_config_create((__bridge CFDictionaryRef) config);
    assert(p1Config != NULL);
    context = p1_create(p1Config, NULL);
    p1_config_free(p1Config);
    assert(context != NULL);

    [self createVideoClock:config];
    [self createVideoSources:config];
    [self createAudioSources:config];

    // Monitor notifications
    __weak P1AppDelegate *weakSelf = self;
    contextFileHandle = [[NSFileHandle alloc] initWithFileDescriptor:p1_fd(context)];
    contextFileHandle.readabilityHandler = ^(NSFileHandle *f) {
        [weakSelf handleContextNotification];
    };

    // Keep context running at all times, so we can see a preview.
    bool ret = p1_start(context);
    assert(ret);
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
    terminating = true;
    if (((P1Object *) context)->state == P1_STATE_IDLE) {
        return NSTerminateNow;
    }
    else {
        p1_stop(context, P1_STOP_ASYNC);
        return NSTerminateLater;
    }
}

- (void)applicationWillTerminate:(NSNotification *)notification
{
    contextFileHandle.readabilityHandler = nil;
    contextFileHandle = nil;

    p1_free(context, P1_FREE_EVERYTHING);
    context = NULL;
}

- (void)createVideoClock:(NSDictionary *)config
{
    NSDictionary *clockConfig = config[@"video"][@"clock"];
    NSString *type = clockConfig[@"type"];

    P1VideoClockFactory *factory = NULL;
    if ([type isEqualToString:@"display"])
        factory = p1_display_video_clock_create;
    assert(factory != NULL);

    P1Config *p1Config = p1_plist_config_create((__bridge CFDictionaryRef) clockConfig);
    assert(p1Config != NULL);
    P1VideoClock *clock = factory(p1Config, NULL);
    p1_config_free(p1Config);
    assert(clock != NULL);

    context->video->clock = clock;
}

- (void)createVideoSources:(NSDictionary *)config
{
    NSArray *sourceConfigs = config[@"video"][@"sources"];
    for (NSDictionary *sourceConfig in sourceConfigs) {
        NSString *type = sourceConfig[@"type"];

        P1VideoSourceFactory *factory = NULL;
        if ([type isEqualToString:@"display"])
            factory = p1_display_video_source_create;
        else if ([type isEqualToString:@"capture"])
            factory = p1_capture_video_source_create;
        assert(factory != NULL);

        P1Config *p1Config = p1_plist_config_create((__bridge CFDictionaryRef) sourceConfig);
        assert(p1Config != NULL);
        P1VideoSource *videoSource = factory(p1Config, NULL);
        p1_config_free(p1Config);
        assert(videoSource != NULL);

        P1Source *source = (P1Source *) videoSource;
        p1_list_before(&context->video->sources, &source->link);
    }
}

- (void)createAudioSources:(NSDictionary *)config
{
    NSArray *sourceConfigs = config[@"audio"][@"sources"];
    for (NSDictionary *sourceConfig in sourceConfigs) {
        NSString *type = sourceConfig[@"type"];

        P1AudioSourceFactory *factory = NULL;
        if ([type isEqualToString:@"input"])
            factory = p1_input_audio_source_create;
        assert(factory != NULL);

        P1Config *p1Config = p1_plist_config_create((__bridge CFDictionaryRef) sourceConfig);
        assert(p1Config != NULL);
        P1AudioSource *audioSource = factory(p1Config, NULL);
        p1_config_free(p1Config);
        assert(audioSource != NULL);

        P1Source *source = (P1Source *) audioSource;
        p1_list_before(&context->audio->sources, &source->link);
    }
}

- (void)handleContextNotification
{
    P1Object *ctxobj = (P1Object *) context;

    P1Notification n;
    p1_read(context, &n);

    if (terminating && n.type == P1_NTYPE_STATE_CHANGE &&
        n.object == ctxobj && n.state_change.state == P1_STATE_IDLE)
        [NSApp replyToApplicationShouldTerminate:TRUE];
}

@end
