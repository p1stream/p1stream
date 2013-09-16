#import "P1PreviewView.h"


@interface P1MainWindowController : NSObject <NSWindowDelegate>

@property (assign) P1Context *context;

@property (strong) IBOutlet NSWindow *window;
@property (weak) IBOutlet P1PreviewView *preview;

- (void)showWindow;
- (void)closeWindow;

@end
