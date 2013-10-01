#import "P1LogWindowController.h"


@implementation P1LogWindowController

- (id)init
{
    return [super initWithWindowNibName:@"LogWindow"];
}

- (void)windowWillClose:(NSNotification *)notification
{
    self.window = nil;
}

@end
