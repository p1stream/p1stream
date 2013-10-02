#import "P1ContextModel.h"
#import "P1PreviewView.h"
#import "P1LogWindowController.h"


@interface P1MainWindowController : NSWindowController <NSWindowDelegate>

@property (assign) P1ContextModel *contextModel;

@property (strong) IBOutlet P1LogWindowController *logWindowControler;
@property (weak) IBOutlet P1PreviewView *preview;
@property (weak) IBOutlet NSButton *connectButton;

@end
