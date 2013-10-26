#import "P1DisplayVideoSourceViewController.h"

#import "P1Graphics.h"


@implementation P1DisplayVideoSourceViewController

- (id)init
{
    return [super initWithNibName:@"DisplayVideoSourceView" bundle:nil];
}

- (void)loadView
{
    [self refreshDisplays:nil];
    [super loadView];
}

- (IBAction)refreshDisplays:(id)sender
{
    NSArray *displays = P1GraphicsGetDisplays();
    if (displays)
        self.displays = displays;
}

@end
