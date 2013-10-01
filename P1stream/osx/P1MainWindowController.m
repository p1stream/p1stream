#import "P1MainWindowController.h"


@implementation P1MainWindowController

- (id)init
{
    return [super initWithWindowNibName:@"MainWindow"];
}

- (void)showWindow:(id)sender
{
    [super showWindow:sender];
    _preview.context = _contextModel.context;
}

- (void)windowWillClose:(NSNotification *)notification
{
    _preview.context = nil;
}

@end
