#import "P1ContextModel.h"

static void (^P1ContextModelNotificationHandler)(NSFileHandle *fh);


@implementation P1ContextModel

- (id)init
{
    P1Context *context = p1_create();
    if (context == NULL)
        return nil;

    self = [super initWithObject:(P1Object *)context name:@"Context"];
    if (self) {
        _logMessages = [NSMutableArray new];
        context->log_fn = logCallback;
        context->log_user_data = (__bridge void *)self;

        _audioModel = [[P1AudioModel alloc] initWithContext:context];
        _videoModel = [[P1VideoModel alloc] initWithContext:context];
        _connectionModel = [[P1ConnectionModel alloc] initWithContext:context];

        if (!_logMessages || !_audioModel || !_videoModel || !_connectionModel) return nil;

        [self reconfigure];
        [self listenForNotifications];
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
    NSUserDefaults *ud = [NSUserDefaults standardUserDefaults];
    NSDictionary *dict = [ud dictionaryForKey:@"general"];

    P1Config *config = p1_plist_config_create(dict);
    if (config == NULL)
        abort();

    p1_config(self.context, config);

    p1_config_free(config);

    [self.audioModel reconfigurePlugins];
    [self.videoModel reconfigurePlugins];
}


- (void)start
{
    bool ret = p1_start(self.context);
    if (!ret)
        abort();
}

- (void)stop
{
    p1_stop(self.context, P1_STOP_ASYNC);
}

- (void)setTarget:(P1TargetState)target;
{
    [NSException raise:@"Cannot set context target"
                format:@"Context should be controlled with start and stop messages."];
}


- (void)listenForNotifications
{
    NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
    P1Context *context = self.context;

    _contextFileHandle = [[NSFileHandle alloc] initWithFileDescriptor:p1_fd(context)];
    _contextFileHandle.readabilityHandler = ^(NSFileHandle *fh) {
        P1Notification n;
        p1_read(context, &n);
        if (n.object != NULL) {
            P1ObjectModel *obj = [P1ObjectModel modelForObject:n.object];

            [obj handleNotification:&n];

            NSValue *box = [NSValue valueWithPointer:&n];
            [nc postNotificationName:@"P1Notification"
                              object:obj
                            userInfo:@{ @"notification": box }];
        }
    };
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

        P1ObjectModel *objectModel = [P1ObjectModel modelForObject:object];
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
