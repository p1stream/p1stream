#import "P1PrefsWindowController.h"


@implementation P1PrefsWindowController

- (id)init
{
    return [super initWithWindowNibName:@"PrefsWindow"];
}

- (void)windowDidLoad
{
    _toolbar.selectedItemIdentifier = @"P1ConnectionPage";
}

- (void)windowWillClose:(NSNotification *)notification
{
    if (_userDefaultsController.hasUnappliedChanges)
        [_userDefaultsController revert:nil];
}

- (IBAction)dummy:(id)sender
{
    // This is apparently needed to make toolbar items clickable.
}

- (IBAction)applySettings:(id)sender
{
    [_userDefaultsController save:sender];

    // FIXME: Apply defaults to context.
}

@end
