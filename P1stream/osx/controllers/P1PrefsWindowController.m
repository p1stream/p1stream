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

- (IBAction)dummy:(NSToolbarItem *)item
{
    // This is apparently needed to make items clickable.
}

@end
