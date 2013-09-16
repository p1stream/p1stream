#import "P1AppDelegate.h"


@implementation P1AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    _terminating = false;

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
    _context = p1_create(p1Config, NULL);
    p1_config_free(p1Config);
    assert(_context != NULL);

    [self createVideoClock:config];
    [self createVideoSources:config];
    [self createAudioSources:config];

    // Monitor notifications
    __weak P1AppDelegate *weakSelf = self;
    _contextFileHandle = [[NSFileHandle alloc] initWithFileDescriptor:p1_fd(_context)];
    _contextFileHandle.readabilityHandler = ^(NSFileHandle *f) {
        [weakSelf handleContextNotification];
    };

    // Keep context running at all times, so we can see a preview
    bool ret = p1_start(_context);
    assert(ret);

    // Show the main window
    _mainWindowController.context = _context;
    [_mainWindowController showWindow];
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

    _context->video->clock = clock;
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
        p1_list_before(&_context->video->sources, &source->link);
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
        p1_list_before(&_context->audio->sources, &source->link);
    }
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
    _terminating = true;

    // Immediate response, but also prevents the preview from showing garbage.
    [_mainWindowController closeWindow];

    // Wait for p1_stop.
    if (((P1Object *) _context)->state == P1_STATE_IDLE) {
        return NSTerminateNow;
    }
    else {
        p1_stop(_context, P1_STOP_ASYNC);
        return NSTerminateLater;
    }
}

- (void)handleContextNotification
{
    P1Object *ctxobj = (P1Object *) _context;

    P1Notification n;
    p1_read(_context, &n);

    // Handle p1_stop result.
    if (_terminating && n.type == P1_NTYPE_STATE_CHANGE &&
        n.object == ctxobj && n.state_change.state == P1_STATE_IDLE)
        [NSApp replyToApplicationShouldTerminate:TRUE];
}

- (void)applicationWillTerminate:(NSNotification *)notification
{
    _contextFileHandle.readabilityHandler = NULL;
    _contextFileHandle = nil;

    p1_free(_context, P1_FREE_EVERYTHING);
    _context = NULL;
}

- (P1Context *)context
{
    return _context;
}

@end
