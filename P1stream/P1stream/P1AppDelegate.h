#import "P1MainWindow.h"
#import "P1GPreview.h"
#import "P1GPipeline.h"


@interface P1AppDelegate : NSObject <NSApplicationDelegate>
{
    P1GPipeline *pipeline;
}

@property (weak) IBOutlet P1GPreview *previewView;

@end
