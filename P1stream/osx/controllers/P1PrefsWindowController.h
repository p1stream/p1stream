#import "P1ContextModel.h"


@interface P1PrefsWindowController : NSWindowController <NSWindowDelegate>

@property (assign) P1ContextModel *contextModel;

@property (retain) IBOutlet NSUserDefaultsController *userDefaultsController;

@property (weak) IBOutlet NSToolbar *toolbar;
@property (weak) IBOutlet NSTabView *tabView;

@end
