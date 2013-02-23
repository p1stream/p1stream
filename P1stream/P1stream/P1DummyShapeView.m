#import "P1DummyShapeView.h"


@implementation P1DummyShapeView

- (void)drawRect:(NSRect)dirtyRect
{
    [[NSColor clearColor] set];
    NSRectFill(dirtyRect);

    // Slighty larger corner radius than P1Preview, to prevent color bleeding.
    [[NSColor whiteColor] set];
    NSBezierPath *path = [NSBezierPath bezierPathWithRoundedRect:self.bounds xRadius:7 yRadius:7];
    [path fill];
}

@end
