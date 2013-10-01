#import "P1ContextModel.h"
#import "P1PreviewView.h"


@interface P1MainWindowController : NSWindowController <NSWindowDelegate>

@property (assign) P1ContextModel *contextModel;

@property (weak) IBOutlet P1PreviewView *preview;

@end
