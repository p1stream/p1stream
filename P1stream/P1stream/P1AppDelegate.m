#import "P1AppDelegate.h"
#import "P1GMainLoop.h"


@implementation P1AppDelegate

@synthesize previewView;

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    [[P1GMainLoop defaultMainLoop] start];

    pipeline = [[P1GPipeline alloc] initWithPreview:previewView];
    [pipeline start];
}

- (void)applicationWillTerminate:(NSNotification *)notification
{
    [pipeline stop];

    [[P1GMainLoop defaultMainLoop] stop];
}

@end
