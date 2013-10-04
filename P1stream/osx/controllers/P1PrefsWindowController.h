#import "P1ContextModel.h"


@interface P1PrefsWindowController : NSWindowController <NSToolbarDelegate>

@property (assign) P1ContextModel *contextModel;

@property (weak) IBOutlet NSToolbar *toolbar;
@property (weak) IBOutlet NSTabView *tabView;

@end
