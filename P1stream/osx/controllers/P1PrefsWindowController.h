#import "P1ContextModel.h"
#import "P1PrefsDictionary.h"


@interface P1PrefsWindowController : NSWindowController <NSWindowDelegate, P1PrefsDictionaryDelegate>

@property (retain) P1ContextModel *contextModel;

@property (retain) NSArray *profileNames;
@property (retain) NSArray *presetNames;
@property (retain) NSArray *tuneNames;

@property (retain) P1PrefsDictionary *generalConfig;
@property (retain) NSMutableArray *audioSourceConfigs;
@property (retain) P1PrefsDictionary *videoClockConfig;
@property (retain) NSMutableArray *videoSourceConfigs;
@property (assign) BOOL isDirty;

@property (retain) NSViewController *audioSourceViewController;
@property (retain) NSViewController *videoSourceViewController;

@property (weak) IBOutlet NSToolbar *toolbar;
@property (weak) IBOutlet NSTabView *tabView;

@property (weak) IBOutlet NSBox *audioSourceViewBox;

- (IBAction)revertPrefs:(id)sender;
- (IBAction)applyPrefs:(id)sender;

- (IBAction)selectedAudioSourceDidChange:(NSTableView *)sender;

@end
