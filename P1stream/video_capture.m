#include <stdio.h>
#include <assert.h>
#include <mach/mach_time.h>
#include <CoreVideo/CoreVideo.h>
#include <CoreMedia/CoreMedia.h>
#include <AVFoundation/AVFoundation.h>

#include "video.h"

// Source state.
typedef struct _P1VideoCaptureSource P1VideoCaptureSource;

struct _P1VideoCaptureSource {
    P1VideoSource super;

    CFTypeRef delegate;
    CFTypeRef session;
};

// Delegate class we use internally for the capture session.
@interface P1VideoCaptureDelegate : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate>
{
    P1VideoCaptureSource *source;
    mach_timebase_info_data_t timebase;
}

- (id)initWithSource:(P1VideoCaptureSource *)_source;

- (void)captureOutput:(AVCaptureOutput *)captureOutput
didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
       fromConnection:(AVCaptureConnection *)connection;

- (void)handleError:(NSNotification *)obj;

@end

// Plugin definition.
static P1VideoSource *p1_video_capture_create();
static void p1_video_capture_free(P1VideoSource *_source);
static bool p1_video_capture_start(P1VideoSource *_source);
static void p1_video_capture_stop(P1VideoSource *_source);

P1VideoPlugin p1_video_capture = {
    .create = p1_video_capture_create,
    .free = p1_video_capture_free,

    .start = p1_video_capture_start,
    .stop = p1_video_capture_stop
};


static P1VideoSource *p1_video_capture_create()
{
    P1VideoCaptureSource *source = calloc(1, sizeof(P1VideoCaptureSource));
    assert(source != NULL);

    P1VideoSource *_source = (P1VideoSource *) source;
    _source->plugin = &p1_video_capture;

    @autoreleasepool {
        P1VideoCaptureDelegate *delegate = [[P1VideoCaptureDelegate alloc] initWithSource:source];

        // Create a capture session, listen for errors.
        AVCaptureSession *session = [[AVCaptureSession alloc] init];
        [[NSNotificationCenter defaultCenter] addObserver:delegate
                                                 selector:@selector(handleError:)
                                                     name:@"AVCaptureSessionRuntimeErrorNotification"
                                                   object:session];

        // Open the default video capture device.
        NSError *error;
        AVCaptureDevice *device = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];
        AVCaptureDeviceInput *input = [AVCaptureDeviceInput deviceInputWithDevice:device error:&error];
        assert(input && [session canAddInput:input]);
        [session addInput:input];

        // Create a video data output.
        dispatch_queue_t dispatch = dispatch_queue_create("video_capture", DISPATCH_QUEUE_SERIAL);
        AVCaptureVideoDataOutput *output = [[AVCaptureVideoDataOutput alloc] init];
        [output setSampleBufferDelegate:delegate queue:dispatch];
        output.videoSettings = @{
            (NSString *) kCVPixelBufferPixelFormatTypeKey: [NSNumber numberWithInt:kCVPixelFormatType_32BGRA]
        };
        assert([session canAddOutput:output]);
        [session addOutput:output];

        // Retain session in state.
        source->delegate = CFBridgingRetain(delegate);
        source->session = CFBridgingRetain(session);
    }

    return _source;
}

static void p1_video_capture_free(P1VideoSource *_source)
{
    P1VideoCaptureSource *source = (P1VideoCaptureSource *) _source;

    CFRelease(source->session);
    CFRelease(source->delegate);
}

static bool p1_video_capture_start(P1VideoSource *_source)
{
    P1VideoCaptureSource *source = (P1VideoCaptureSource *) _source;

    @autoreleasepool {
        AVCaptureSession *session = (__bridge AVCaptureSession *)source->session;
        [session startRunning];
    }

    return true;
}

static void p1_video_capture_stop(P1VideoSource *_source)
{
    P1VideoCaptureSource *source = (P1VideoCaptureSource *) _source;

    @autoreleasepool {
        AVCaptureSession *session = (__bridge AVCaptureSession *)source->session;
        [session stopRunning];
    }
}

@implementation P1VideoCaptureDelegate

- (id)initWithSource:(P1VideoCaptureSource *)_source
{
    self = [super init];
    if (self) {
        source = _source;
        mach_timebase_info(&timebase);
    }
    return self;
}

- (void)captureOutput:(AVCaptureOutput *)captureOutput
didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
       fromConnection:(AVCaptureConnection *)connection
{
    P1VideoSource *_source = (P1VideoSource *) source;

    // Calculate mach time of this sample.
    CMTime cmtime = CMSampleBufferGetPresentationTimeStamp(sampleBuffer);
    int64_t nanoscale = 1000000000 / cmtime.timescale;
    int64_t time = cmtime.value * nanoscale * timebase.denom / timebase.numer;
    // Get the image data.
    CVPixelBufferRef pixbuf = CMSampleBufferGetImageBuffer(sampleBuffer);
    assert(pixbuf != NULL);
    IOSurfaceRef surface = CVPixelBufferGetIOSurface(pixbuf);
    if (surface != NULL) {
        p1_video_frame_iosurface(_source, time, surface);
    }
    else {
        CVPixelBufferLockBaseAddress(pixbuf, kCVPixelBufferLock_ReadOnly);
        int width = (int) CVPixelBufferGetWidth(pixbuf);
        int height = (int) CVPixelBufferGetHeight(pixbuf);
        void *data = CVPixelBufferGetBaseAddress(pixbuf);
        p1_video_frame_raw(_source, time, width, height, data);
        CVPixelBufferUnlockBaseAddress(pixbuf, kCVPixelBufferLock_ReadOnly);
    }
}

- (void)handleError:(NSNotification *)obj
{
    NSLog(@"%@\n", obj.userInfo);
    assert(0);
}

@end
