#import "P1LogWindowController.h"


@implementation P1LogWindowController

- (void)showWindow
{
    if (_window == nil) {
        BOOL ret = [[NSBundle mainBundle] loadNibNamed:@"LogWindow"
                                                 owner:self
                                       topLevelObjects:NULL];
        assert(ret == TRUE);
    }

    [_window makeKeyAndOrderFront:self];
}

- (void)closeWindow
{
    [_window close];
}

- (void)windowWillClose:(NSNotification *)notification
{
    _window = nil;
}

@end
