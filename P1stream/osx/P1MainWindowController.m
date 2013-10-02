#import "P1MainWindowController.h"


@implementation P1MainWindowController

- (id)init
{
    return [super initWithWindowNibName:@"MainWindow"];
}

- (void)windowDidLoad
{
    _logWindowControler.contextModel = _contextModel;
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

- (IBAction)viewLog:(id)sender {
    [_logWindowControler showWindow:sender];
}

@end
