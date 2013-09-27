#import "P1MainWindowController.h"
#import "P1ContextModel.h"


@interface P1AppDelegate : NSObject <NSApplicationDelegate>
{
    bool _terminating;
}

@property (retain) P1ContextModel *contextModel;

@property (weak) IBOutlet P1MainWindowController *mainWindowController;

@end
