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
@property (retain) NSViewController *videoClockViewController;
@property (retain) NSViewController *videoSourceViewController;

@property (weak) IBOutlet NSToolbar *toolbar;
@property (weak) IBOutlet NSTabView *tabView;

@property (weak) IBOutlet NSTableView *audioSourcesTableView;
@property (weak) IBOutlet NSPopUpButton *videoClockPopUpButton;
@property (weak) IBOutlet NSTableView *videoSourcesTableView;

@property (weak) IBOutlet NSBox *audioSourceViewBox;
@property (weak) IBOutlet NSBox *videoClockViewBox;
@property (weak) IBOutlet NSBox *videoSourceViewBox;

- (IBAction)revertPrefs:(id)sender;
- (IBAction)applyPrefs:(id)sender;

- (IBAction)selectedAudioSourceDidChange:(id)sender;
- (IBAction)removeSelectedAudioSource:(id)sender;
- (IBAction)addInputAudioSource:(id)sender;

- (IBAction)createDisplayVideoClock:(id)sender;

- (IBAction)selectedVideoSourceDidChange:(id)sender;
- (IBAction)removeSelectedVideoSource:(id)sender;
- (IBAction)addDisplayVideoSource:(id)sender;
- (IBAction)addCaptureVideoSource:(id)sender;

@end
