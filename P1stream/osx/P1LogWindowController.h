#import "P1ContextModel.h"


@interface P1LogWindowController : NSObject <NSWindowDelegate>

@property (assign) P1ContextModel *contextModel;

@property (retain) IBOutlet NSWindow *window;

- (void)showWindow;
- (void)closeWindow;

@end
