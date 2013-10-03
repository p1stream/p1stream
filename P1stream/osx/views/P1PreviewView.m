#import "P1PreviewView.h"


@implementation P1PreviewView

- (id)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    if (self) {
        _colorSpace = CGColorSpaceCreateDeviceRGB();
        if (_colorSpace == NULL) {
            NSLog(@"Failed to get RGB colorspace");
            return NULL;
        }

        CALayer *layer = [CALayer layer];
        layer.backgroundColor = CGColorGetConstantColor(kCGColorBlack);

        self.layer = layer;
        self.wantsLayer = TRUE;
    }
    return self;
}

- (void)dealloc
{
    self.context = NULL;

    if (_colorSpace != NULL) {
        CFRelease(_colorSpace);
        _colorSpace = NULL;
    }
}

- (P1Context *)context
{
    return _context;
}

- (void)setContext:(P1Context *)context
{
    if (context == _context)
        return;

    if (_context)
        setVideoPreviewCallback(_context->video, NULL, NULL);
    if (_dataProvider)
        CFRelease(_dataProvider);

    _context = context;
    self.aspect = 0;
    _lastData = NULL;
    _dataProvider = NULL;
    self.layer.contents = nil;

    if (_context)
        setVideoPreviewCallback(_context->video, videoPreviewCallback, (__bridge void *)self);
}

static void setVideoPreviewCallback(P1Video *video, P1VideoPreviewCallback fn, void *user_data)
{
    P1Object *videoobj = (P1Object *)video;

    p1_object_lock(videoobj);

    video->preview_fn = fn;
    video->preview_user_data = user_data;

    p1_object_unlock(videoobj);
}

static void videoPreviewCallback(size_t width, size_t height, uint8_t *data, void *user_data)
{
    @autoreleasepool {
        P1PreviewView *self = (__bridge P1PreviewView *)user_data;

        self.aspect = (float)width / (float)height;

        [self updatePreviewWithData:data width:width height:height];
    }
}

- (void)setAspect:(float)aspect
{
    if (aspect == _aspect)
        return;
    _aspect = aspect;

    if (aspect == 0) {
        if (_constraint) {
            [self removeConstraint:_constraint];
            _constraint = nil;
        }
    }
    else {
        _constraint = [NSLayoutConstraint constraintWithItem:self
                                                   attribute:NSLayoutAttributeWidth
                                                   relatedBy:NSLayoutRelationEqual
                                                      toItem:self
                                                   attribute:NSLayoutAttributeHeight
                                                  multiplier:aspect
                                                    constant:0];
        [self addConstraint:_constraint];
    }
}

- (void)updatePreviewWithData:(uint8_t *)data width:(size_t)width height:(size_t)height
{
    size_t stride = width * 4;
    size_t size = stride * height;

    if (data != _lastData) {
        if (_dataProvider)
            CFRelease(_dataProvider);

        _dataProvider = CGDataProviderCreateWithData(NULL, data, size, NULL);
        if (_dataProvider == NULL) {
            NSLog(@"Failed to create data provider for preview image");
            return;
        }

        _lastData = data;
    }

    CGImageRef cgImage = CGImageCreate(width, height, 8, 32, stride, _colorSpace,
                                       kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst,
                                       _dataProvider, NULL, FALSE, kCGRenderingIntentDefault);

    NSImage *image = [[NSImage alloc] initWithCGImage:cgImage size:NSZeroSize];

    CFRelease(cgImage);

    [self.layer performSelectorOnMainThread:@selector(setContents:) withObject:image waitUntilDone:FALSE];
}

@end
