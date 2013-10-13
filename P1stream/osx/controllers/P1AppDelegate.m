#import "P1AppDelegate.h"


@implementation P1AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    // Register config defaults
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [defaults registerDefaults:@{
        @"url": @"rtmp://localhost/app/test",
        @"video-width": @1280,
        @"video-height": @720,
        @"x264-bitrate": @4096,
        @"x264-keyint-sec": @2,
        @"x264-profile": @"baseline",
        @"x264-preset": @"veryfast",
        @"x264-x-nal-hrd": @"cbr",
        @"audio-sources": @[
            @{
                @"name": @"Audio input source",
                @"type": @"input"
            }
        ],
        @"video-clock": @{
            @"name": @"Display video clock",
            @"type": @"display"
        },
        @"video-sources": @[
            @{
                @"name": @"Display video source",
                @"type": @"display"
            }
        ]
    }];

    NSUserDefaultsController *defaultsController = [NSUserDefaultsController sharedUserDefaultsController];
    defaultsController.appliesImmediately = FALSE;

    // Create context.
    _contextModel = [[P1ContextModel alloc] init];
    _mainWindowController.contextModel  = _contextModel;
    _logWindowController.contextModel   = _contextModel;
    _prefsWindowController.contextModel = _contextModel;

    // Show the main window.
    [_mainWindowController showWindow:nil];

    // Monitor context state for clean exit.
    _terminating = false;
    [_contextModel addObserver:self forKeyPath:@"state" options:0 context:nil];

    // Monitor connection state.
    [_contextModel.connectionModel addObserver:self forKeyPath:@"error" options:0 context:nil];

    // Monitor other notifications so we can restart objects.
    NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
    [nc addObserver:self
           selector:@selector(restartObjectsIfNeeded:)
               name:@"P1Notification" object:nil];

    // Start disconnected.
    _contextModel.connectionModel.target = P1_TARGET_IDLE;
    [_contextModel start];
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
    _terminating = true;

    // Immediate response, but also prevents the preview from showing garbage.
    [_mainWindowController close];

    // Wait for p1_stop.
    if (_contextModel.currentState == P1_STATE_IDLE) {
        return NSTerminateNow;
    }
    else {
        [_contextModel stop];
        return NSTerminateLater;
    }
}

- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary *)change
                       context:(void *)context
{
    // Handle p1_stop result.
    if (object == _contextModel && [keyPath isEqualToString:@"state"]) {
        if (_terminating && _contextModel.currentState == P1_STATE_IDLE) {
            [NSApp replyToApplicationShouldTerminate:TRUE];
            return;
        }
    }

    // If our connection breaks, reset to idle state.
    P1ObjectModel *connectionModel = _contextModel.connectionModel;
    if (object == connectionModel && [keyPath isEqualToString:@"error"]) {
        if (connectionModel.error && connectionModel.target != P1_TARGET_IDLE)
            connectionModel.target = P1_TARGET_IDLE;
    }
}

- (void)restartObjectsIfNeeded:(NSNotification *)notification
{
    P1Notification *n;

    NSValue *box = notification.userInfo[@"notification"];
    [box getValue:&n];

    P1Context *context = n->object->ctx;
    P1Object *contextObject = (P1Object *)context;
    P1ContextModel *contextModel = (__bridge P1ContextModel *)contextObject->user_data;

    // When the connection is idle, restart objects as needed.
    if (contextModel.connectionModel.currentState == P1_STATE_IDLE) {
        [contextModel.audioModel restartIfNeeded];
        [contextModel.videoModel restartIfNeeded];
        [contextModel.videoModel.clockModel restartIfNeeded];
    }

    // We can restart sources whenever.
    for (P1ObjectModel *sourceModel in contextModel.audioModel.sourceModels)
        [sourceModel restartIfNeeded];
    for (P1ObjectModel *sourceModel in contextModel.videoModel.sourceModels)
        [sourceModel restartIfNeeded];
}

- (IBAction)visitWebsite:(id)sender
{
    NSURL *url = [NSURL URLWithString:@"http://p1stream.com/"];
    [[NSWorkspace sharedWorkspace] openURL:url];
}

@end
