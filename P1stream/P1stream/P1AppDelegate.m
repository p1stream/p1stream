#import "P1AppDelegate.h"
#import "P1GLibLoop.h"


@implementation P1AppDelegate

@synthesize mainWindowController;

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    [[P1GLibLoop defaultMainLoop] start];

    [mainWindowController showWindow:self];
}

- (void)applicationWillTerminate:(NSNotification *)notification
{
    [[P1GLibLoop defaultMainLoop] stop];
}

@end
