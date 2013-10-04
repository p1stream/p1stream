#import "P1ContextModel.h"
#import "P1MainWindowController.h"
#import "P1LogWindowController.h"
#import "P1PrefsWindowController.h"


@interface P1AppDelegate : NSObject <NSApplicationDelegate>
{
    bool _terminating;
    P1ContextModel *_contextModel;
}

@property (weak) IBOutlet P1MainWindowController *mainWindowController;
@property (weak) IBOutlet P1LogWindowController *logWindowController;
@property (weak) IBOutlet P1PrefsWindowController *prefsWindowController;

@end
