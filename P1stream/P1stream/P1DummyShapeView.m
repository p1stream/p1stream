#import "P1DummyShapeView.h"


@implementation P1DummyShapeView

- (void)drawRect:(NSRect)dirtyRect
{
    [[NSGraphicsContext currentContext] saveGraphicsState];

    [[NSColor clearColor] set];
    NSRectFill(dirtyRect);

    // Slighty larger corner radius than P1Preview, to prevent color bleeding.
    [[NSColor whiteColor] set];
    NSRectClip(dirtyRect);
    NSBezierPath *path = [NSBezierPath bezierPathWithRoundedRect:self.bounds xRadius:7 yRadius:7];
    [path fill];

    [[NSGraphicsContext currentContext] restoreGraphicsState];
}

@end
