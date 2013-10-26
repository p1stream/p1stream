#import "P1DisplayVideoClockViewController.h"

#import "P1Graphics.h"


@implementation P1DisplayVideoClockViewController

- (id)init
{
    return [super initWithNibName:@"DisplayVideoClockView" bundle:nil];
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
