#import "P1ContextModel.h"


@interface P1PrefsWindowController : NSWindowController <NSWindowDelegate>

@property (retain) NSArray *profileNames;
@property (retain) NSArray *presetNames;
@property (retain) NSArray *tuneNames;

@property (assign) P1ContextModel *contextModel;

@property (retain) IBOutlet NSUserDefaultsController *userDefaultsController;
@property (retain) IBOutlet NSArrayController *audioSourcesController;

@property (weak) IBOutlet NSToolbar *toolbar;
@property (weak) IBOutlet NSTabView *tabView;

@property (retain) NSViewController *audioSourceViewController;
@property (weak) IBOutlet NSBox *audioSourceViewBox;

@end
