#import "P1AppDelegate.h"
#import "P1GLibLoop.h"


@implementation P1AppDelegate

@synthesize previewView;

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    [[P1GLibLoop defaultMainLoop] start];

    pipeline = [[P1Pipeline alloc] initWithPreview:previewView];
    [pipeline start];
}

- (void)applicationWillTerminate:(NSNotification *)notification
{
    [pipeline stop];

    [[P1GLibLoop defaultMainLoop] stop];
}

@end
