#import "P1ContextModel.h"


@interface P1PrefsWindowController : NSWindowController <NSWindowDelegate>

@property (retain) P1ContextModel *contextModel;

@property (retain) NSArray *profileNames;
@property (retain) NSArray *presetNames;
@property (retain) NSArray *tuneNames;

@property (retain) NSMutableDictionary *generalConfig;
@property (retain) NSMutableArray *audioSourceConfigs;
@property (retain) NSMutableDictionary *videoClockConfig;
@property (retain) NSMutableArray *videoSourceConfigs;

@property (retain) NSViewController *audioSourceViewController;
@property (retain) NSViewController *videoSourceViewController;

@property (weak) IBOutlet NSToolbar *toolbar;
@property (weak) IBOutlet NSTabView *tabView;

@property (weak) IBOutlet NSTableView *audioSourcesTable;
@property (weak) IBOutlet NSBox *audioSourceViewBox;

- (IBAction)revertSettings:(id)sender;
- (IBAction)applySettings:(id)sender;

@end
