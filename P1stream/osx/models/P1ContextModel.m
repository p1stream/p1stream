#import "P1ContextModel.h"

static void (^P1ContextModelNotificationHandler)(NSFileHandle *fh);


@implementation P1ContextModel

- (id)init
{
    P1Context *context = p1_create();
    if (context == NULL)
        return nil;

    self = [super initWithObject:(P1Object *) context];
    if (self) {
        _logMessages = [NSMutableArray new];
        context->log_fn = logCallback;
        context->log_user_data = (__bridge void *)self;

        _audioModel = [[P1AudioModel alloc] initWithObject:(P1Object *)context->audio];
        _videoModel = [[P1VideoModel alloc] initWithObject:(P1Object *)context->video];
        _connectionModel = [[P1ConnectionModel alloc] initWithObject:(P1Object *)context->conn];

        if (!_logMessages || !_audioModel || !_videoModel || !_connectionModel) return nil;

        [self reconfigure];

        // FIXME: Move this to reconfigure, compare objects.
        NSDictionary *dict = [[NSUserDefaults standardUserDefaults] dictionaryRepresentation];
        if (![self createVideoClock:dict]) return nil;
        if (![self createVideoSources:dict]) return nil;
        if (![self createAudioSources:dict]) return nil;

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
        assert(self.currentState == P1_STATE_IDLE);
        p1_free(self.context, P1_FREE_EVERYTHING);
    }
}


- (P1Context *)context
{
    return (P1Context *)self.object;
}


- (void)reconfigure
{
    NSDictionary *dict = [[NSUserDefaults standardUserDefaults] dictionaryRepresentation];

    P1Config *config = p1_plist_config_create(dict);
    if (config == NULL)
        abort();

    p1_config(self.context, config);

    p1_config_free(config);
}


- (void)start
{
    _restart = FALSE;

    bool ret = p1_start(self.context);
    if (!ret)
        abort();
}

- (void)stop
{
    _restart = FALSE;

    p1_stop(self.context, P1_STOP_ASYNC);
}

// Context needs some special logic to restart.
- (void)restart
{
    if (self.currentState == P1_STATE_IDLE) {
        [self start];
    }
    else {
        [self stop];
        _restart = TRUE;
    }
}


// We handle this using notifications.
+ (BOOL)automaticallyNotifiesObserversForKey:(NSString *)key
{
    if ([key isEqualToString:@"needsConnectionRestart"])
        return NO;
    else
        return [super automaticallyNotifiesObserversForKey:key];
}

- (BOOL)needsConnectionRestart
{
    return _needsConnectionRestart;
}


- (BOOL)createVideoClock:(NSDictionary *)dict
{
    NSDictionary *clockDict = dict[@"video-clock"];
    NSString *type = clockDict[@"type"];

    P1VideoClockFactory *factory = NULL;
    if ([type isEqualToString:@"display"])
        factory = p1_display_video_clock_create;
    if (factory == NULL)
        return FALSE;

    P1Config *config = p1_plist_config_create(clockDict);
    if (config == NULL)
        return FALSE;

    P1VideoClock *videoClock = factory(self.context);
    if (videoClock == NULL) {
        p1_config_free(config);
        return FALSE;
    }

    p1_video_clock_config(videoClock, config);
    p1_config_free(config);

    P1ObjectModel *model = [[P1ObjectModel alloc] initWithObject:(P1Object *)videoClock];
    if (!model) {
        p1_plugin_free((P1Plugin *)videoClock);
        return FALSE;
    }

    NSString *name = clockDict[@"name"];
    if (name)
        model.name = name;

    self.videoModel.clockModel = model;

    return TRUE;
}

- (BOOL)createVideoSources:(NSDictionary *)dict
{
    NSArray *sourcesArray = dict[@"video-sources"];
    for (NSDictionary *sourceDict in sourcesArray) {
        NSString *type = sourceDict[@"type"];

        P1VideoSourceFactory *factory = NULL;
        if ([type isEqualToString:@"display"])
            factory = p1_display_video_source_create;
        else if ([type isEqualToString:@"capture"])
            factory = p1_capture_video_source_create;
        if (factory == NULL)
            return FALSE;

        P1Config *config = p1_plist_config_create(sourceDict);
        if (config == NULL)
            return FALSE;

        P1VideoSource *videoSource = factory(self.context);
        if (videoSource == NULL) {
            p1_config_free(config);
            return FALSE;
        }

        p1_video_source_config(videoSource, config);
        p1_config_free(config);

        P1ObjectModel *model = [[P1ObjectModel alloc] initWithObject:(P1Object *)videoSource];
        if (!model) {
            p1_plugin_free((P1Plugin *)videoSource);
            return FALSE;
        }

        NSString *name = sourceDict[@"name"];
        if (name)
            model.name = name;

        [self.videoModel insertObject:model
                inSourceModelsAtIndex:[self.videoModel.sourceModels count]];
    }

    return TRUE;
}

- (BOOL)createAudioSources:(NSDictionary *)dict
{
    NSArray *sourcesArray = dict[@"audio-sources"];
    for (NSDictionary *sourceDict in sourcesArray) {
        NSString *type = sourceDict[@"type"];

        P1AudioSourceFactory *factory = NULL;
        if ([type isEqualToString:@"input"])
            factory = p1_input_audio_source_create;
        if (factory == NULL)
            return FALSE;

        P1Config *config = p1_plist_config_create(sourceDict);
        if (config == NULL)
            return FALSE;

        P1AudioSource *audioSource = factory(self.context);
        if (audioSource == NULL) {
            p1_config_free(config);
            return FALSE;
        }

        p1_audio_source_config(audioSource, config);
        p1_config_free(config);

        P1ObjectModel *model = [[P1ObjectModel alloc] initWithObject:(P1Object *)audioSource];
        if (!model) {
            p1_plugin_free((P1Plugin *)audioSource);
            return FALSE;
        }

        NSString *name = sourceDict[@"name"];
        if (name)
            model.name = name;

        [self.audioModel insertObject:model
                inSourceModelsAtIndex:[self.audioModel.sourceModels count]];
    }

    return TRUE;
}

- (BOOL)listenForNotifications
{
    NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
    P1Context *context = self.context;

    _contextFileHandle = [[NSFileHandle alloc] initWithFileDescriptor:p1_fd(context)];
    if (!_contextFileHandle)
        return FALSE;

    _contextFileHandle.readabilityHandler = ^(NSFileHandle *fh) {
        P1Notification n;
        p1_read(context, &n);
        if (n.object != NULL) {
            P1ObjectModel *obj = (__bridge P1ObjectModel *) n.object->user_data;
            [obj handleNotification:&n];

            [obj.contextModel checkNeedsRestart];

            NSValue *box = [NSValue valueWithPointer:&n];
            [nc postNotificationName:@"P1Notification"
                              object:obj
                            userInfo:@{ @"notification": box }];
        }
    };

    return TRUE;
}

- (void)handleNotification:(P1Notification *)n
{
    [super handleNotification:n];

    if (_restart && n->state.current == P1_STATE_IDLE)
        [self start];
}

- (void)checkNeedsRestart
{
    [self willChangeValueForKey:@"needsConnectionRestart"];
    _needsConnectionRestart = _audioModel.needsRestart ||
                              _videoModel.needsRestart ||
                              _videoModel.clockModel.needsRestart ||
                              _connectionModel.needsRestart;
    [self didChangeValueForKey:@"needsConnectionRestart"];
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
