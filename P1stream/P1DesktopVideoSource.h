#import "P1VideoCanvas.h"


@interface P1DesktopVideoSource : NSObject <P1VideoSource>
{
@private
    P1VideoSourceSlot *slot;
    CGDirectDisplayID displayID;
    CGRect captureArea;

    CGDisplayStreamRef displayStream;
    uint32_t lastSeed;
}

@property (retain) id delegate;
@property (retain) P1VideoSourceSlot *slot;
@property CGDirectDisplayID displayID;
@property CGRect captureArea;

@end
