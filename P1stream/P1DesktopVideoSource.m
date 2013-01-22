#import "P1DesktopVideoSource.h"


@implementation P1DesktopVideoSource

@synthesize delegate;

- (id)init
{
    self = [super init];
    if (self) {
        // Colorspace used for the texture.
        colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);
        if (!colorSpace) return nil;

        // Display link used to sync to frames.
		CVReturn ret = CVDisplayLinkCreateWithActiveCGDisplays(&displayLink);
		if (ret != kCVReturnSuccess) {
            NSLog(@"Failed to create desktop source display link.");
            displayLink = NULL;
            return nil;
        }
        CVDisplayLinkSetOutputCallback(displayLink, &displayLinkCallback, (__bridge void *)self);
    }
    return self;
}

- (void)dealloc
{
    [self cleanupState];

    if (bitmapContext) {
        CGContextRelease(bitmapContext);
    }

    if (textureData) {
        free(textureData);
    }

    if (colorSpace) {
        CGColorSpaceRelease(colorSpace);
    }

    if (displayLink) {
        CVDisplayLinkRelease(displayLink);
    }
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
        
        [slot.context makeCurrentContext];

        // Permanently bind this context to the output texture.
        glBindTexture(GL_TEXTURE_RECTANGLE, slot.textureName);

        // We'll be using texture ranges, and optimize for streaming texture data.
		glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_STORAGE_HINT_APPLE, GL_STORAGE_SHARED_APPLE);
        glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_TRUE);

        // Flush errors at this point.
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

        // Update the display link.
        CVReturn ret = CVDisplayLinkSetCurrentCGDisplay(displayLink, displayID);
        if (ret != kCVReturnSuccess) {
            NSLog(@"Failed to set display on display link (%d)", ret);
            displayID = kCGNullDirectDisplay;
            return;
        }
        
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
        if (bitmapContext) {
            CGContextRelease(bitmapContext);
            bitmapContext = NULL;
            free(textureData);
            textureData = NULL;
        }
        
        captureArea = captureArea_;

        // Calculate texture memory area size.
        const CGSize size = captureArea.size;
        const size_t bpp = 4;
        textureSize = size.width * size.height * bpp;

        // Repeatedly used in CGContextDrawImage.
        textureBounds = CGRectMake(0, 0, size.width, size.height);

        // Texture scratch buffer.
        textureData = malloc(textureSize);
        if (!textureData) {
            NSLog(@"Failed to allocate desktop source texture data.");
            return;
        }

        // Quartz context used to export to our scratch buffer.
        bitmapContext = CGBitmapContextCreate(textureData, size.width, size.height,
                                              8, size.width * bpp, colorSpace,
                                              kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host);
        if (!bitmapContext) {
            NSLog(@"Failed to create desktop source bitmap.");
            free(textureData);
            textureData = NULL;
            return;
        }
        
        [self setupState];
    }
}

// Private. Set up state linking parameters together.
- (void)setupState
{
    // Check if all properties are set.
    if (!slot) return;
    if (displayID == kCGNullDirectDisplay) return;
    if (!bitmapContext) return;

    // Set texture range to our scratch memory.
    [slot.context makeCurrentContext];
    glTextureRangeAPPLE(GL_TEXTURE_RECTANGLE, (GLsizei)textureSize, textureData);
    checkAndLogGLError(@"desktop source setup");

    // Start capturing.
    CVReturn ret = CVDisplayLinkStart(displayLink);
    if (ret != kCVReturnSuccess) {
        NSLog(@"Failed to start display link.");
    }
}

// Private. Clean up above state. Also called from dealloc.
- (void)cleanupState
{
    // Stop capturing.
    if (displayLink && CVDisplayLinkIsRunning(displayLink)) {
        CVDisplayLinkStop(displayLink);
    }

    // Remove the texture range.
    if (slot) {
        [slot.context makeCurrentContext];
        glTextureRangeAPPLE(GL_TEXTURE_RECTANGLE, 0, NULL);
    }
}

- (BOOL)hasClock;
{
    @synchronized(self) {
        return CVDisplayLinkIsRunning(displayLink);
    }
}

// Private. Capture and upload a frame.
- (BOOL)captureFrame
{
    @synchronized(self) {
        // Capture.
        CGImageRef image = CGDisplayCreateImageForRect(displayID, captureArea);
        if (!image) {
            NSLog(@"Failed to capture desktop source frame.");
            return NO;
        }

        // Convert.
        CGContextDrawImage(bitmapContext, textureBounds, image);

        // Upload.
        [slot.context makeCurrentContext];
        glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGBA,
                     (GLsizei)textureBounds.size.width, (GLsizei)textureBounds.size.height, 0,
                     GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, textureData);

#ifdef DEBUG_EACH_GL_FRAME
        checkAndLogGLError(@"desktop source capture");
#endif

        CGImageRelease(image);
        return YES;
    }
}

// Private. On display link tick.
- (BOOL)clockTick
{
    @synchronized(self) {
        // Capture a frame.
        BOOL success = [self captureFrame];
        
        // Drive the clock.
        if (delegate != nil)
            [delegate videoSourceClockTick];

        return success;
    }
}

static CVReturn displayLinkCallback(CVDisplayLinkRef displayLink, const CVTimeStamp *inNow,
                                 const CVTimeStamp *inOutputTime, CVOptionFlags flagsIn,
                                 CVOptionFlags *flagsOut, void *displayLinkContext)
{
    @autoreleasepool {
        P1DesktopVideoSource *self = (__bridge P1DesktopVideoSource *)displayLinkContext;
        BOOL success = [self clockTick];
        return success ? kCVReturnSuccess : kCVReturnError;
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
