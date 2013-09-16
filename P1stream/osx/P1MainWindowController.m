#import "P1MainWindowController.h"


@implementation P1MainWindowController

- (void)showWindow
{
    if (_window == nil) {
        BOOL ret = [[NSBundle mainBundle] loadNibNamed:@"MainWindow"
                                                 owner:self
                                       topLevelObjects:NULL];
        assert(ret == TRUE);

        _preview.context = _context;
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
