#import "P1ContextModel.h"
#import "P1MainWindowController.h"


@interface P1AppDelegate : NSObject <NSApplicationDelegate>
{
    bool _terminating;
    P1ContextModel *_contextModel;
}

@property (weak) IBOutlet P1MainWindowController *mainWindowController;

@end
