#import "P1AppDelegate.h"


@implementation P1AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    // Create context.
    self.contextModel = [[P1ContextModel alloc] init];

    // Show the main window.
    _mainWindowController.contextModel = _contextModel;
    [_mainWindowController showWindow];

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
    [_mainWindowController closeWindow];

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

@end
