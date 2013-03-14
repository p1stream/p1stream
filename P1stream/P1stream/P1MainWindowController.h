#import "P1Preview.h"
#import "P1Pipeline.h"


@interface P1MainWindowController : NSWindowController <NSWindowDelegate>
{
    P1Preview *preview;
    NSArray *constraints;

    P1Pipeline *pipeline;
}

@end
