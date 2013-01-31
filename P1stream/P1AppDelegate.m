#import "P1AppDelegate.h"
#import "P1DesktopVideoSource.h"
#import "P1HALAudioSource.h"


@implementation P1AppDelegate

@synthesize videoPreview;

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    canvas = [[P1VideoCanvas alloc] init];
    if (!canvas) {
        NSLog(@"Failed to create video canvas.");
        return;
    }
    canvas.delegate = self;
    canvas.frameSize = CGSizeMake(512, 384);
    
    // FIXME
    P1DesktopVideoSource *desktopSource = [[P1DesktopVideoSource alloc] init];
    if (!desktopSource) {
        NSLog(@"Failed to create desktop video source.");
        return;
    }
    desktopSource.displayID = CGMainDisplayID();
    desktopSource.captureArea = CGRectMake(0, 0, 2560, 1440);
    [canvas addSource:desktopSource withDrawArea:CGRectMake(-1, -1, 2, 2)];

    mixer = [[P1AudioMixer alloc] init];
    if (!mixer) {
        NSLog(@"Failed to create audio mixer.");
        return;
    }

    P1HALAudioSource *halSource = [[P1HALAudioSource alloc] init];
    if (!halSource) {
        NSLog(@"Failed to create HAL audio source");
        return;
    }
    
}

- (void *)getVideoCanvasOutputBufferARGB:(size_t)size withDimensions:(CGSize)dim
{
    @synchronized(self) {
        if (outputSize != size || !CGSizeEqualToSize(dim, outputDim)) {
            outputSize = 0;
            outputDim = CGSizeMake(0, 0);

            outputBuffer = realloc(outputBuffer, size);
            if (!outputBuffer) {
                NSLog(@"Failed to allocate memory for video canvas output buffer.");
                return NULL;
            }

            outputSize = size;
            outputDim = dim;
        }

        return outputBuffer;
    }
}

- (void)videoCanvasFrameARGB
{
    NSImageView *videoPreview_ = self.videoPreview;
    if (!videoPreview_) return;

    dispatch_async(dispatch_get_main_queue(), ^{
        CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, outputBuffer, outputSize, NULL);
        if (provider) {
            CGColorSpaceRef colorspace = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);
            if (colorspace) {
                CGImageRef image = CGImageCreate(outputDim.width, outputDim.height, 8, 32, outputDim.width * 4, colorspace,
                                                 kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host,
                                                 provider, NULL, FALSE, kCGRenderingIntentDefault);
                if (image) {
                    NSImage *outputImage = [[NSImage alloc] initWithCGImage:image size:outputDim];
                    if (outputImage) {
                        videoPreview_.image = outputImage;
                    }
                    CGImageRelease(image);
                }
                CGColorSpaceRelease(colorspace);
            }
            CGDataProviderRelease(provider);
        }
    });
}

- (void)dealloc
{
    if (outputBuffer) {
        free(outputBuffer);
    }
}

@end
