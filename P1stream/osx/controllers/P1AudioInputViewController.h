@interface P1AudioInputViewController : NSViewController

@property (retain) NSArray *devices;

- (IBAction)refreshDevices:(id)sender;

@end
