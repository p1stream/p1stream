#import "P1AppDelegate.h"
#import "P1DesktopVideoSource.h"
#import "P1InputAudioSource.h"
#include <CoreAudio/CoreAudio.h>


@implementation P1AppDelegate

@synthesize videoPreview;

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    // FIXME: Most of this is dummy code.

    // Create desktop video source for default display.
    P1DesktopVideoSource *desktopSource = [[P1DesktopVideoSource alloc] init];
    if (!desktopSource) {
        NSLog(@"Failed to create desktop video source.");
        return;
    }
    desktopSource.displayID = CGMainDisplayID();
    desktopSource.captureArea = CGRectMake(0, 0, 2560, 1440);

    // Create input audio source for default device.
    UInt32 propertySize;
    P1InputAudioSource *halSource = [[P1InputAudioSource alloc] init];
    if (!halSource) {
        NSLog(@"Failed to create HAL audio source.");
        return;
    }
    AudioDeviceID audioDevice;
    propertySize = sizeof(AudioDeviceID);
    const AudioObjectPropertyAddress inputDeviceProp = {
        .mSelector = kAudioHardwarePropertyDefaultInputDevice,
        .mScope = kAudioObjectPropertyScopeGlobal,
        .mElement = kAudioObjectPropertyElementMaster
    };
    OSStatus err = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                              &inputDeviceProp, 0, NULL,
                                              &propertySize, &audioDevice);
    if (err) {
        NSLog(@"Failed to get default input device (%d)", err);
        return;
    }
    propertySize = sizeof(CFStringRef);
    CFStringRef audioDeviceUID;
    const AudioObjectPropertyAddress deviceUidProp = {
        .mElement = kAudioObjectPropertyElementMaster,
        .mScope = kAudioObjectPropertyScopeGlobal,
        .mSelector = kAudioDevicePropertyDeviceUID
    };
    err = AudioObjectGetPropertyData(audioDevice,
                                     &deviceUidProp, 0, NULL,
                                     &propertySize, &audioDeviceUID);
    if (err) {
        NSLog(@"Failed to get input device UID. (%d)", err);
    }
    halSource.deviceUID = CFBridgingRelease(audioDeviceUID);

    // Create video canvas.
    canvas = [[P1VideoCanvas alloc] init];
    if (!canvas) {
        NSLog(@"Failed to create video canvas.");
        return;
    }
    canvas.delegate = self;
    canvas.frameSize = CGSizeMake(512, 384);

    // Create audio mixer.
    mixer = [[P1AudioMixer alloc] init];
    if (!mixer) {
        NSLog(@"Failed to create audio mixer.");
        return;
    }
    mixer.delegate = self;

    // Add sources.
    [canvas addSource:desktopSource withDrawArea:CGRectMake(-1, -1, 2, 2)];
    [mixer addSource:halSource withVolume:1.0f];
}

- (void *)getVideoCanvasOutputBufferARGB:(size_t)size withDimensions:(CGSize)dimensions
{
    @synchronized(self) {
        if (videoBufferSize != size || !CGSizeEqualToSize(dimensions, videoDimensions)) {
            videoBufferSize = 0;
            videoDimensions = CGSizeMake(0, 0);

            videoBuffer = realloc(videoBuffer, size);
            if (!videoBuffer) {
                NSLog(@"Failed to allocate memory for video canvas output buffer.");
                return NULL;
            }

            videoBufferSize = size;
            videoDimensions = dimensions;
        }

        return videoBuffer;
    }
}

- (void)videoCanvasFrameARGB
{
    NSImageView *videoPreview_ = self.videoPreview;
    if (!videoPreview_) return;

    dispatch_async(dispatch_get_main_queue(), ^{
        @synchronized(self) {
            CGImageRef image = NULL;

            CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, videoBuffer, videoBufferSize, NULL);
            CGColorSpaceRef colorspace = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);
            if (provider && colorspace) {
                image = CGImageCreate(videoDimensions.width, videoDimensions.height,
                                      8, 32, videoDimensions.width * 4, colorspace,
                                      kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host,
                                      provider, NULL, FALSE, kCGRenderingIntentDefault);
            }
            if (provider)
                CGDataProviderRelease(provider);
            if (colorspace)
                CGColorSpaceRelease(colorspace);

            if (image) {
                NSImage *outputImage = [[NSImage alloc] initWithCGImage:image size:videoDimensions];
                if (outputImage) {
                    videoPreview_.image = outputImage;
                }
                CGImageRelease(image);
            }
        }
    });
}

- (void *)getAudioMixerOutputBuffer:(size_t)size
{
    @synchronized(self) {
        if (audioBufferSize != size) {
            audioBufferSize = 0;

            audioBuffer = realloc(audioBuffer, size);
            if (!audioBuffer) {
                NSLog(@"Failed to allocate memory for audio mixer output buffer.");
                return NULL;
            }

            audioBufferSize = size;
        }

        return audioBuffer;
    }
}

- (void)audioMixerBufferReady
{
    // FIXME
}

- (void)dealloc
{
    if (videoBuffer) {
        free(videoBuffer);
    }
}

@end
