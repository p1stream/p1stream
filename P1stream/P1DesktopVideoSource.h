#import "P1VideoCanvas.h"


@interface P1DesktopVideoSource : NSObject <P1VideoSource>
{
@private
    CVDisplayLinkRef displayLink;
    CGColorSpaceRef colorSpace;

    P1VideoSourceSlot *slot;

    CGDirectDisplayID displayID;

    CGRect captureArea;
    CGRect textureBounds;
    size_t textureSize;
    GLubyte *textureData;
    CGContextRef bitmapContext;
}

@property (retain) id delegate;
@property (retain) P1VideoSourceSlot *slot;
@property CGDirectDisplayID displayID;
@property CGRect captureArea;

@end
