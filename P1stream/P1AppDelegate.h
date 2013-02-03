#import <Cocoa/Cocoa.h>
#import "P1VideoCanvas.h"
#import "P1AudioMixer.h"


@interface P1AppDelegate : NSObject <NSApplicationDelegate, P1VideoCanvasDelegate, P1AudioMixerDelegate>
{
    P1VideoCanvas *canvas;
    P1AudioMixer *mixer;
    
    void *videoBuffer;
    size_t videoBufferSize;
    CGSize videoDimensions;

    void *audioBuffer;
    size_t audioBufferSize;
}

@property (weak) IBOutlet NSImageView *videoPreview;

@end
