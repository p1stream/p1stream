#import "P1MainWindowController.h"


@implementation P1MainWindowController

- (id)init
{
    return [super initWithWindowNibName:@"MainWindow"];
}

- (void)windowDidLoad
{
    self.window.movableByWindowBackground = TRUE;

    [self.window addObserver:self
                  forKeyPath:@"visible"
                     options:0
                     context:nil];
}

- (BOOL)windowShouldClose:(id)sender
{
    [NSApp terminate:sender];
    return TRUE;
}

- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary *)change
                       context:(void *)context
{
    if (object == self.window && [keyPath isEqualToString:@"visible"])
        [self updatePreview];
}

- (void)updatePreview
{
    if (!_preview)
        return;

    if ([self.window isVisible])
        _preview.context = _contextModel.context;
    else
        _preview.context = nil;
}

@end
