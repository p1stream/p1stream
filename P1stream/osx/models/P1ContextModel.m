#import "P1ContextModel.h"

static void (^P1ContextModelNotificationHandler)(NSFileHandle *fh);


@implementation P1ContextModel

- (id)init
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

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
                        @"type": @"input"
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

    P1Context *context = p1_create(p1Config, NULL);
    p1_config_free(p1Config);
    assert(context != NULL);

    self = [super initWithObject:(P1Object *) context];
    if (self) {
        _logMessages = [NSMutableArray new];
        context->log_fn = logCallback;
        context->log_user_data = (__bridge void *)self;

        _audioModel = [[P1ObjectModel alloc] initWithObject:(P1Object *)context->audio];
        _videoModel = [[P1ObjectModel alloc] initWithObject:(P1Object *)context->video];
        _connectionModel = [[P1ObjectModel alloc] initWithObject:(P1Object *)context->conn];

        _objects = [[NSMutableArray alloc] init];

        if (!_logMessages || !_audioModel || !_videoModel || !_connectionModel || !_objects) return nil;

        if (![self createVideoClock:config]) return nil;
        if (![self createVideoSources:config]) return nil;
        if (![self createAudioSources:config]) return nil;

        if (![self listenForNotifications]) return nil;
    }
    else {
        p1_free(context, P1_FREE_EVERYTHING);
    }
    return self;
}

- (void)dealloc
{
    P1Context *context = self.context;
    if (context != NULL) {
        assert(self.state == P1_STATE_IDLE);
        p1_free(self.context, P1_FREE_EVERYTHING);
    }
}


- (P1Context *)context
{
    return (P1Context *)self.object;
}


- (void)start
{
    bool ret = p1_start(self.context);
    assert(ret);
}

- (void)stop
{
    p1_stop(self.context, P1_STOP_ASYNC);
}


- (BOOL)createVideoClock:(NSDictionary *)config
{
    NSDictionary *clockConfig = config[@"video"][@"clock"];
    NSString *type = clockConfig[@"type"];

    P1VideoClockFactory *factory = NULL;
    if ([type isEqualToString:@"display"])
        factory = p1_display_video_clock_create;
    if (factory == NULL)
        return FALSE;

    P1Config *p1Config = p1_plist_config_create((__bridge CFDictionaryRef) clockConfig);
    if (p1Config == NULL)
        return FALSE;

    P1VideoClock *clock = factory(p1Config, NULL);
    p1_config_free(p1Config);
    if (clock == NULL)
        return FALSE;

    P1ObjectModel *model = [[P1ObjectModel alloc] initWithObject:(P1Object *)clock];
    if (!model) {
        p1_plugin_free((P1Plugin *)clock);
        return FALSE;
    }

    self.context->video->clock = clock;
    [_objects addObject:model];

    return TRUE;
}

- (BOOL)createVideoSources:(NSDictionary *)config
{
    NSArray *sourceConfigs = config[@"video"][@"sources"];
    for (NSDictionary *sourceConfig in sourceConfigs) {
        NSString *type = sourceConfig[@"type"];

        P1VideoSourceFactory *factory = NULL;
        if ([type isEqualToString:@"display"])
            factory = p1_display_video_source_create;
        else if ([type isEqualToString:@"capture"])
            factory = p1_capture_video_source_create;
        if (factory == NULL)
            return FALSE;

        P1Config *p1Config = p1_plist_config_create((__bridge CFDictionaryRef) sourceConfig);
        if (p1Config == NULL)
            return FALSE;

        P1VideoSource *videoSource = factory(p1Config, NULL);
        p1_config_free(p1Config);
        if (videoSource == NULL)
            return FALSE;

        P1ObjectModel *model = [[P1ObjectModel alloc] initWithObject:(P1Object *)videoSource];
        if (!model) {
            p1_plugin_free((P1Plugin *)videoSource);
            return FALSE;
        }

        P1Source *source = (P1Source *)videoSource;
        p1_list_before(&self.context->video->sources, &source->link);
        [_objects addObject:model];
    }

    return TRUE;
}

- (BOOL)createAudioSources:(NSDictionary *)config
{
    NSArray *sourceConfigs = config[@"audio"][@"sources"];
    for (NSDictionary *sourceConfig in sourceConfigs) {
        NSString *type = sourceConfig[@"type"];

        P1AudioSourceFactory *factory = NULL;
        if ([type isEqualToString:@"input"])
            factory = p1_input_audio_source_create;
        if (factory == NULL)
            return FALSE;

        P1Config *p1Config = p1_plist_config_create((__bridge CFDictionaryRef) sourceConfig);
        if (p1Config == NULL)
            return FALSE;

        P1AudioSource *audioSource = factory(p1Config, NULL);
        p1_config_free(p1Config);
        if (audioSource == NULL)
            return FALSE;

        P1ObjectModel *model = [[P1ObjectModel alloc] initWithObject:(P1Object *)audioSource];
        if (!model) {
            p1_plugin_free((P1Plugin *)audioSource);
            return FALSE;
        }

        P1Source *source = (P1Source *)audioSource;
        p1_list_before(&self.context->audio->sources, &source->link);
        [_objects addObject:model];
    }

    return TRUE;
}

- (BOOL)listenForNotifications
{
    P1Context *context = self.context;

    _contextFileHandle = [[NSFileHandle alloc] initWithFileDescriptor:p1_fd(context)];
    if (!_contextFileHandle)
        return FALSE;

    _contextFileHandle.readabilityHandler = ^(NSFileHandle *fh) {
        P1Notification n;
        p1_read(context, &n);
        if (n.type != P1_NTYPE_NULL) {
            P1ObjectModel *obj = (__bridge P1ObjectModel *) n.object->user_data;
            [obj handleNotification:&n];
        }
    };

    return TRUE;
}


- (NSArray *)logMessages
{
    return _logMessages;
}

- (void)insertObject:(P1LogMessage *)object inLogMessagesAtIndex:(NSUInteger)index
{
    [_logMessages insertObject:object atIndex:index];
}

- (void)removeObjectFromLogMessagesAtIndex:(NSUInteger)index
{
    [_logMessages removeObjectAtIndex:index];
}


static void logCallback(P1Object *object, P1LogLevel level, const char *format, va_list args, void *userData)
{
    @autoreleasepool {
        P1ContextModel *self = (__bridge P1ContextModel *)userData;

        char buffer[2048];
        vsnprintf(buffer, sizeof(buffer), format, args);
        NSString *message = [NSString stringWithUTF8String:buffer];

        P1ObjectModel *objectModel = (__bridge P1ObjectModel *)object->user_data;
        P1LogMessage *logMessage = [[P1LogMessage alloc] initWithModel:objectModel
                                                              andLevel:level
                                                            andMessage:message];

        [self performSelectorOnMainThread:@selector(addLogMessage:)
                               withObject:logMessage
                            waitUntilDone:FALSE];
    }
}

- (void)addLogMessage:(P1LogMessage *)logMessage
{
    [self insertObject:logMessage inLogMessagesAtIndex:[_logMessages count]];
}

@end
