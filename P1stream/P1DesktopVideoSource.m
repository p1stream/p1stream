#import "P1DesktopVideoSource.h"


@implementation P1DesktopVideoSource

@synthesize delegate;

- (void)dealloc
{
    [self cleanupState];
}

- (P1VideoSourceSlot *)slot
{
    @synchronized(self) {
        return slot;
    }
}

- (void)setSlot:(P1VideoSourceSlot *)slot_
{
    @synchronized(self) {
        if (slot_ == slot) return;

        [self cleanupState];
        
        slot = slot_;

        // Permanently bind this context to the output texture.
        [slot.context makeCurrentContext];
        glBindTexture(GL_TEXTURE_RECTANGLE, slot.textureName);
        checkAndLogGLError(@"desktop source slot init");

        [self setupState];
    }
}

- (CGDirectDisplayID)displayID
{
    @synchronized(self) {
        return displayID;
    }
}

- (void)setDisplayID:(CGDirectDisplayID)displayID_
{
    @synchronized(self) {
        if (displayID_ == displayID) return;

        [self cleanupState];

        displayID = displayID_;
        
        [self setupState];
    }
}

- (CGRect)captureArea
{
    @synchronized(self) {
        return captureArea;
    }
}

- (void)setCaptureArea:(CGRect)captureArea_
{
    @synchronized(self) {
        [self cleanupState];
        
        captureArea = captureArea_;
        
        [self setupState];
    }
}

// Private. Set up state linking parameters together.
- (void)setupState
{
    // Check if all properties are set.
    if (!slot) return;
    if (displayID == kCGNullDirectDisplay) return;
    if (captureArea.size.width <= 0 && captureArea.size.height <= 0) return;

    // FIXME: Does not account for HiDPI displays.

    // Preallocate the texture.
    [slot.context makeCurrentContext];
    glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGBA,
                 captureArea.size.width, captureArea.size.height, 0,
                 GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, captureArea.size.width);
    if (checkAndLogGLError(@"desktop source setup")) return;

    // Start capturing.
    NSDictionary *properties = @{
        (__bridge NSString *)kCGDisplayStreamSourceRect : CFBridgingRelease(CGRectCreateDictionaryRepresentation(captureArea))
    };
    displayStream = CGDisplayStreamCreateWithDispatchQueue(
        displayID, captureArea.size.width, captureArea.size.height,
        k32BGRAPixelFormat, (__bridge CFDictionaryRef)properties,
        dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
        ^(CGDisplayStreamFrameStatus status, uint64_t displayTime, IOSurfaceRef frameSurface, CGDisplayStreamUpdateRef updateRef) {
            if (status == kCGDisplayStreamFrameStatusStopped) return;

            // Check for change.
            uint32_t seed = IOSurfaceGetSeed(frameSurface);
            if (status == kCGDisplayStreamFrameStatusFrameComplete && seed != lastSeed) {
                @synchronized(self) {
                    // Capture a frame.
                    IOSurfaceLock(frameSurface, kIOSurfaceLockReadOnly, NULL);
                    uint32_t *data = IOSurfaceGetBaseAddress(frameSurface);

                    [slot.context makeCurrentContext];

                    // Update dirty areas.
                    size_t numRects;
                    const CGRect *i = CGDisplayStreamUpdateGetRects(updateRef, kCGDisplayStreamUpdateReducedDirtyRects, &numRects);
                    const CGRect *end = i + numRects;
                    while (i != end) {
                        size_t offset = i->origin.y * captureArea.size.width + i->origin.x;
                        glTexSubImage2D(GL_TEXTURE_RECTANGLE, 0,
                                        i->origin.x, i->origin.y, i->size.width, i->size.height,
                                        GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, data + offset);
                        i++;
                    }

                    IOSurfaceUnlock(frameSurface, kIOSurfaceLockReadOnly, &lastSeed);

#ifdef DEBUG_EACH_GL_FRAME
                    checkAndLogGLError(@"desktop source capture");
#endif
                }
            }
            
            // Drive the clock.
            if (delegate != nil) {
                [delegate videoSourceClockTick];
            }
        }
    );
    if (!displayStream) {
        NSLog(@"Failed to create display stream.");
        return;
    }

    CGError error = CGDisplayStreamStart(displayStream);
    if (error != kCGErrorSuccess) {
        NSLog(@"Failed to start display stream. (%d)", error);
        CFRelease(displayStream);
        displayStream = NULL;
    }
}

// Private. Clean up above state. Also called from dealloc.
- (void)cleanupState
{
    // Stop capturing.
    if (displayStream) {
        CFRelease(displayStream);
        displayStream = NULL;
        lastSeed = 0;
    }
}

- (BOOL)hasClock;
{
    @synchronized(self) {
        return displayStream != NULL;
    }
}

- (NSDictionary *)serialize
{
    @synchronized(self) {
        return @{
            @"displayID" : [NSNumber numberWithUnsignedInt:displayID],
            @"captureArea" : CFBridgingRelease(CGRectCreateDictionaryRepresentation(captureArea)),
        };
    }
}

- (void)deserialize:(NSDictionary *)dict
{
    @synchronized(self) {
        self.displayID = [[dict objectForKey:@"displayID"] unsignedIntValue];

        CGRect captureArea_;
        NSDictionary *captureAreaDict = [dict objectForKey:@"captureArea"];
        CGRectMakeWithDictionaryRepresentation((__bridge CFDictionaryRef)captureAreaDict, &captureArea_);
        self.captureArea = captureArea_;
    }
}

@end
