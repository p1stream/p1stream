#import "P1ContextModel.h"


@interface P1PrefsWindowController : NSWindowController <NSWindowDelegate>

@property (retain) NSArray *profileNames;
@property (retain) NSArray *presetNames;
@property (retain) NSArray *tuneNames;

@property (assign) P1ContextModel *contextModel;

@property (retain) IBOutlet NSUserDefaultsController *userDefaultsController;

@property (weak) IBOutlet NSToolbar *toolbar;
@property (weak) IBOutlet NSTabView *tabView;

@end
