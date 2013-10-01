#import "P1MainWindowController.h"


@implementation P1MainWindowController

- (id)init
{
    return [super initWithWindowNibName:@"MainWindow"];
}

- (void)windowDidLoad
{
    _preview.context = _contextModel.context;
}

- (void)windowWillClose:(NSNotification *)notification
{
    self.window = nil;
}

@end
