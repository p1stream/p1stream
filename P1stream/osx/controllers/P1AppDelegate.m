#import "P1AppDelegate.h"


@implementation P1AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    _terminating = FALSE;

    // Register config defaults
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [defaults registerDefaults:@{
        @"general": @{
            @"url": @"rtmp://localhost/app/test",
            @"video-width": @1280,
            @"video-height": @720,
            @"x264-bitrate": @4096,
            @"x264-keyint-sec": @2,
            @"x264-profile": @"baseline",
            @"x264-preset": @"veryfast",
            @"x264-x-nal-hrd": @"cbr",
        },
        @"audio-sources": @[],
        @"video-clock": @{
            @"uuid": @"00000000-0000-0000-0000-000000000000",
            @"type": @"display"
        },
        @"video-sources": @[]
    }];

    NSUserDefaultsController *defaultsController = [NSUserDefaultsController sharedUserDefaultsController];
    defaultsController.appliesImmediately = FALSE;

    // Create context.
    _contextModel = [[P1ContextModel alloc] init];
    _mainWindowController.contextModel  = _contextModel;
    _logWindowController.contextModel   = _contextModel;
    _prefsWindowController.contextModel = _contextModel;

    // Act on notifications.
    NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
    [nc addObserver:self
           selector:@selector(restartObjectsIfNeeded:)
               name:@"P1Notification" object:nil];

    // Show the main window.
    [_mainWindowController showWindow:nil];

    // Start disconnected.
    _contextModel.connectionModel.target = P1_TARGET_IDLE;
    [_contextModel start];
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
    _terminating = TRUE;

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

- (void)restartObjectsIfNeeded:(NSNotification *)notification
{
    P1Notification *n;

    NSValue *box = notification.userInfo[@"notification"];
    [box getValue:&n];

    // In case we ever have multiple contexts, check here.
    P1ContextModel *contextModel = [P1ObjectModel modelForObject:(P1Object *)n->object->ctx];
    if (contextModel != _contextModel)
        return;

    // Handle p1_stop result.
    if (_terminating && contextModel.currentState == P1_STATE_IDLE) {
        [NSApp replyToApplicationShouldTerminate:TRUE];
        return;
    }

    P1AudioModel *audioModel = contextModel.audioModel;
    P1VideoModel *videoModel = contextModel.videoModel;
    P1ConnectionModel *connectionModel = contextModel.connectionModel;

    // If our connection breaks, reset to idle state.
    if (connectionModel.error && connectionModel.target != P1_TARGET_IDLE)
        connectionModel.target = P1_TARGET_IDLE;

    if (contextModel.connectionModel.currentState == P1_STATE_IDLE) {
        // When the connection is idle, restart objects as needed.
        self.needsConnectionRestart = FALSE;

        P1PluginModel *clockModel = videoModel.clockModel;
        if (videoModel.hasNewClockPending) {
            if (!clockModel.isPendingRemoval)
                [clockModel remove];
            else if (clockModel.currentState == P1_STATE_IDLE)
                [videoModel swapNewClock];
        }
        else {
            [videoModel.clockModel restartIfNeeded];
        }

        P1AudioModel *audioModel = contextModel.audioModel;
        [audioModel restartIfNeeded];

        P1VideoModel *videoModel = contextModel.videoModel;
        [videoModel restartIfNeeded];
    }
    else {
        // Otherwise, determine if the user needs to stop the connection.
        self.needsConnectionRestart =
            videoModel.hasNewClockPending ||
            videoModel.clockModel.needsRestart ||
            audioModel.needsRestart ||
            videoModel.needsRestart ||
            connectionModel.needsRestart;
    }
}

- (IBAction)visitWebsite:(id)sender
{
    NSURL *url = [NSURL URLWithString:@"http://p1stream.com/"];
    [[NSWorkspace sharedWorkspace] openURL:url];
}

@end
