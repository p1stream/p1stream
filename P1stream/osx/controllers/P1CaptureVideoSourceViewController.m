#import "P1CaptureVideoSourceViewController.h"

#import <AVFoundation/AVFoundation.h>


@implementation P1CaptureVideoSourceViewController

- (id)init
{
    return [super initWithNibName:@"CaptureVideoSourceView" bundle:nil];
}

- (void)loadView
{
    [self refreshDevices:nil];
    [super loadView];
}

- (IBAction)refreshDevices:(id)sender
{
    self.devices = [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];
}

@end
