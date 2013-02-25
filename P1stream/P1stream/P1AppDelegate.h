#import "P1Preview.h"
#import "P1Pipeline.h"


@interface P1AppDelegate : NSObject <NSApplicationDelegate>
{
    P1Pipeline *pipeline;
}

@property (weak) IBOutlet P1Preview *previewView;

@end
