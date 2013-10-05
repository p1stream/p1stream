#import "P1AppDelegate.h"


@implementation P1AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

    // Register config defaults
    [defaults registerDefaults:@{
        @"url": @"rtmp://localhost/app/test",
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
    [_contextModel.connectionModel addObserver:self forKeyPath:@"state" options:0 context:nil];

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
    if (_contextModel.state == P1_STATE_IDLE) {
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
        if (_terminating && _contextModel.state == P1_STATE_IDLE)
            [NSApp replyToApplicationShouldTerminate:TRUE];
    }

    // If our connection breaks, reset to idle state.
    P1ObjectModel *connectionModel = _contextModel.connectionModel;
    if (object == connectionModel && [keyPath isEqualToString:@"state"]) {
        if (connectionModel.state == P1_STATE_HALTED) {
            [connectionModel lock];
            connectionModel.target = P1_TARGET_IDLE;
            [connectionModel clearHalt];
            [connectionModel unlock];
        }
    }
}

- (IBAction)visitWebsite:(id)sender
{
    NSURL *url = [NSURL URLWithString:@"http://p1stream.com/"];
    [[NSWorkspace sharedWorkspace] openURL:url];
}

@end