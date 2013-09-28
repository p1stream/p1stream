#import "P1AppDelegate.h"


@implementation P1AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    // Create and start context. Start disconnected.
    self.contextModel = [[P1ContextModel alloc] init];
    _contextModel.connectionModel.target = P1_TARGET_IDLE;
    [_contextModel start];

    // Monitor context state for clean exit.
    _terminating = false;
    [_contextModel addObserver:self forKeyPath:@"state" options:0 context:nil];

    // Show the main window.
    _mainWindowController.contextModel = _contextModel;
    [_mainWindowController showWindow];
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
}

@end
