#include "p1stream.h"

#include <pthread.h>
#include <CoreVideo/CoreVideo.h>
#include <CoreMedia/CoreMedia.h>
#include <AVFoundation/AVFoundation.h>

typedef struct _P1CaptureVideoSource P1CaptureVideoSource;


struct _P1CaptureVideoSource {
    P1VideoSource super;

    CFTypeRef delegate;
    CFTypeRef session;

    CVPixelBufferRef frame;
};

static bool p1_capture_video_source_init(P1CaptureVideoSource *cvsrc);
static void p1_capture_video_source_start(P1Plugin *pel);
static void p1_capture_video_source_stop(P1Plugin *pel);
static bool p1_capture_video_source_frame(P1VideoSource *vsrc);
static void p1_capture_video_source_kill_session(P1CaptureVideoSource *cvsrc);


// Delegate class we use internally for the capture session.

@interface P1VideoCaptureDelegate : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate>
{
    P1CaptureVideoSource *cvsrc;
}

- (id)initWithSource:(P1CaptureVideoSource *)_source;

- (void)captureOutput:(AVCaptureOutput *)captureOutput
didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
       fromConnection:(AVCaptureConnection *)connection;

- (void)handleError:(NSNotification *)n;

@end


P1VideoSource *p1_capture_video_source_create()
{
    P1CaptureVideoSource *cvsrc = calloc(1, sizeof(P1CaptureVideoSource));

    if (cvsrc != NULL) {
        if (!p1_capture_video_source_init(cvsrc)) {
            free(cvsrc);
            return NULL;
        }
    }

    return (P1VideoSource *) cvsrc;
}

static bool p1_capture_video_source_init(P1CaptureVideoSource *cvsrc)
{
    P1VideoSource *vsrc = (P1VideoSource *) cvsrc;
    P1Plugin *pel = (P1Plugin *) cvsrc;

    if (!p1_video_source_init(vsrc))
        return false;

    pel->start = p1_capture_video_source_start;
    pel->stop = p1_capture_video_source_stop;
    vsrc->frame = p1_capture_video_source_frame;

    return true;
}

static void p1_capture_video_source_start(P1Plugin *pel)
{
    P1Object *obj = (P1Object *) pel;
    P1CaptureVideoSource *cvsrc = (P1CaptureVideoSource *) pel;

#define P1_CVSRC_ERROR {                            \
    obj->state.current = P1_STATE_IDLE;             \
    obj->state.flags |= P1_FLAG_ERROR;              \
    p1_object_notify(obj);                          \
    return;                                         \
}

    @autoreleasepool {
        P1VideoCaptureDelegate *delegate = [[P1VideoCaptureDelegate alloc] initWithSource:cvsrc];

        // Create a capture session, .
        AVCaptureSession *session = [[AVCaptureSession alloc] init];

        // Listen for errors.
        NSNotificationCenter *notif_center = [NSNotificationCenter defaultCenter];
        [notif_center addObserver:delegate
                         selector:@selector(handleError:)
                             name:@"AVCaptureSessionRuntimeErrorNotification"
                           object:session];

        if (!delegate || !session || !notif_center) {
            p1_log(obj, P1_LOG_ERROR, "Failed to setup capture session");
            P1_CVSRC_ERROR;
        }

        // Open the default video capture device.
        NSError *error;
        AVCaptureDevice *device = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];
        AVCaptureDeviceInput *input = [AVCaptureDeviceInput deviceInputWithDevice:device error:&error];
        if (!input || ![session canAddInput:input]) {
            p1_log(obj, P1_LOG_ERROR, "Failed to open capture device");
            p1_log_ns_error(obj, P1_LOG_ERROR, error);
            P1_CVSRC_ERROR;
        }
        [session addInput:input];

        // Create a video data output.
        dispatch_queue_t dispatch = dispatch_queue_create("video_capture", DISPATCH_QUEUE_SERIAL);
        AVCaptureVideoDataOutput *output = [[AVCaptureVideoDataOutput alloc] init];
        [output setSampleBufferDelegate:delegate queue:dispatch];
        output.videoSettings = @{
            (NSString *) kCVPixelBufferPixelFormatTypeKey: [NSNumber numberWithInt:kCVPixelFormatType_32BGRA]
        };
        if (!output || ![session canAddOutput:output]) {
            p1_log(obj, P1_LOG_ERROR, "Failed to setup capture output");
            P1_CVSRC_ERROR;
        }
        [session addOutput:output];

        // Retain session in state.
        cvsrc->delegate = CFBridgingRetain(delegate);
        cvsrc->session = CFBridgingRetain(session);

        // Start. This is sync, unfortunately.
        p1_object_unlock(obj);
        [session startRunning];
        p1_object_lock(obj);
    }

    // We may have gotten an error notification during startRunning.
    if (!(obj->state.flags & P1_FLAG_ERROR)) {
        obj->state.current = P1_STATE_RUNNING;
        p1_object_notify(obj);
    }

#undef P1_CVSRC_HALT
}

static void p1_capture_video_source_stop(P1Plugin *pel)
{
    P1Object *obj = (P1Object *) pel;
    P1CaptureVideoSource *cvsrc = (P1CaptureVideoSource *) pel;

    @autoreleasepool {
        p1_capture_video_source_kill_session(cvsrc);
    }

    obj->state.current = P1_STATE_IDLE;
    p1_object_notify(obj);
}

static bool p1_capture_video_source_frame(P1VideoSource *vsrc)
{
    P1CaptureVideoSource *cvsrc = (P1CaptureVideoSource *) vsrc;
    CVPixelBufferRef frame;

    frame = cvsrc->frame;
    if (frame) {
        IOSurfaceRef surface = CVPixelBufferGetIOSurface(frame);
        if (surface != NULL) {
            return p1_video_source_frame_iosurface(vsrc, surface);
        }
        else {
            CVPixelBufferLockBaseAddress(frame, kCVPixelBufferLock_ReadOnly);
            int width = (int) CVPixelBufferGetWidth(frame);
            int height = (int) CVPixelBufferGetHeight(frame);
            void *data = CVPixelBufferGetBaseAddress(frame);
            p1_video_source_frame(vsrc, width, height, data);
            CVPixelBufferUnlockBaseAddress(frame, kCVPixelBufferLock_ReadOnly);
        }
    }

    return true;
}

static void p1_capture_video_source_kill_session(P1CaptureVideoSource *cvsrc)
{
    P1Object *obj = (P1Object *) cvsrc;
    AVCaptureSession *session = (__bridge AVCaptureSession *) cvsrc->session;

    // This is sync, unfortunately.
    p1_object_unlock(obj);
    [session stopRunning];
    p1_object_lock(obj);

    if (cvsrc->frame)
        CFRelease(cvsrc->frame);
    CFRelease(cvsrc->session);
    CFRelease(cvsrc->delegate);
}


@implementation P1VideoCaptureDelegate

- (id)initWithSource:(P1CaptureVideoSource *)_cvsrc
{
    self = [super init];
    if (self) {
        cvsrc = _cvsrc;
    }
    return self;
}

- (void)captureOutput:(AVCaptureOutput *)captureOutput
didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
       fromConnection:(AVCaptureConnection *)connection
{
    P1Object *obj = (P1Object *) cvsrc;

    p1_object_lock(obj);

    if (cvsrc->frame)
        CFRelease(cvsrc->frame);

    if (obj->state.current != P1_STATE_RUNNING) {
        p1_object_unlock(obj);
        return;
    }

    CVPixelBufferRef frame = CMSampleBufferGetImageBuffer(sampleBuffer);
    if (frame == NULL) {
        p1_log(obj, P1_LOG_ERROR, "Failed to get image buffer for capture source frame");

        obj->state.current = P1_STATE_STOPPING;
        obj->state.flags |= P1_FLAG_ERROR;
        p1_object_notify(obj);

        p1_capture_video_source_kill_session(cvsrc);

        obj->state.current = P1_STATE_IDLE;
        p1_object_notify(obj);
    }
    else {
        CFRetain(frame);
        cvsrc->frame = frame;
    }

    p1_object_unlock(obj);
}

- (void)handleError:(NSNotification *)n
{
    P1Object *obj = (P1Object *) cvsrc;

    p1_object_lock(obj);

    p1_log(obj, P1_LOG_ERROR, "Error in capture session");
    NSError *err = [n.userInfo objectForKey:AVCaptureSessionErrorKey];
    p1_log_ns_error(obj, P1_LOG_ERROR, err);

    p1_capture_video_source_kill_session(cvsrc);

    obj->state.current = P1_STATE_IDLE;
    obj->state.flags |= P1_FLAG_ERROR;
    p1_object_notify(obj);

    p1_object_unlock(obj);
}

@end
