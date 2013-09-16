#import "P1MainWindowController.h"


@interface P1AppDelegate : NSObject <NSApplicationDelegate>
{
    bool _terminating;

    P1Context *_context;
    NSFileHandle *_contextFileHandle;
}

- (P1Context *)context;

@property (weak) IBOutlet P1MainWindowController *mainWindowController;

@end
