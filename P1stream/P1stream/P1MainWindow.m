#import "P1MainWindow.h"


@implementation P1MainWindow

- (id)initWithContentRect:(NSRect)contentRect
                styleMask:(NSUInteger)styleMask
                  backing:(NSBackingStoreType)backing
                    defer:(BOOL)flag
{
    styleMask = NSBorderlessWindowMask | NSResizableWindowMask;
    self = [super initWithContentRect:contentRect
                            styleMask:styleMask
                              backing:backing
                                defer:flag];
    if (self) {
        self.opaque = FALSE;
        self.movableByWindowBackground = TRUE;
    }
    return self;
}

- (BOOL)canBecomeKeyWindow
{
    return TRUE;
}

@end
