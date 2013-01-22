#import <Cocoa/Cocoa.h>
#import "P1VideoCanvas.h"
#import "P1AudioMixer.h"


@interface P1AppDelegate : NSObject <NSApplicationDelegate, P1VideoCanvasDelegate>
{
    P1VideoCanvas *canvas;
    
    void *outputBuffer;
    size_t outputSize;
    CGSize outputDim;
}

@property (weak) IBOutlet NSImageView *videoPreview;

- (void *)getVideoCanvasOutputBufferARGB:(size_t)size withDimensions:(CGSize)dim;
- (void)videoCanvasFrameARGB;

@end
