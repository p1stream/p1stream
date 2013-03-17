#define P1_VIDEO_CANVAS_NUM_BUFFERS 2

@class P1VideoSourceSlot;


@protocol P1VideoSourceDelegate <NSObject>

@required
- (void)videoSourceClockTick;

@end


@protocol P1VideoSource <NSObject>

// Video canvas slot.
@required
- (void)setSlot:(P1VideoSourceSlot *)slot;

// Clock source.
@required
- (BOOL)hasClock;
@optional
- (void)setDelegate:(id)delegate;

// Serialization.
@required
- (NSDictionary *)serialize;
- (void)deserialize:(NSDictionary *)dict;

@end


@interface P1VideoSourceSlot : NSObject

@property (retain, nonatomic, readonly) id<P1VideoSource> source;
@property (retain, nonatomic, readonly) NSOpenGLContext *context;
@property (nonatomic, readonly) GLuint textureName;
@property CGRect drawArea;

- (id)initForSource:(id<P1VideoSource>)source withContext:(NSOpenGLContext *)context withDrawArea:(CGRect)drawArea;

@end


@protocol P1VideoCanvasDelegate <NSObject>

@optional
- (void *)getVideoCanvasOutputBufferARGB:(size_t)size withDimensions:(CGSize)dimensions;
- (void)videoCanvasFrameARGB;

@optional
- (void *)getVideoCanvasOutputBufferYUV:(size_t)size withDimensions:(CGSize)dimensions;
- (void)videoCanvasFrameYUV;

@end


// The canvas combines video sources into a single image.
@interface P1OldVideoCanvas : NSObject <P1VideoSourceDelegate>
{
    NSOpenGLPixelFormat *pixelFormat;
    NSOpenGLContext *context;
    dispatch_semaphore_t semaphore;
    GLuint vertexBufferName;
    GLuint vertexArrayName;
    GLuint frameBufferName;
    GLuint shaderProgram;

    CGSize frameSize;
    size_t outputSizeARGB;
    size_t outputSizeYUV;
    cl_ndrange clRange;

    struct P1VideoCanvasBuffer {
        BOOL inuse;
        GLuint renderBufferName;
        cl_image clInput;
        cl_uchar* clOutput;
    } buffers[P1_VIDEO_CANVAS_NUM_BUFFERS];

    id<P1VideoSource> clockSource;
}

@property (retain, readonly) NSMutableArray *slots;
@property (retain) id delegate;
@property CGSize frameSize;

- (P1VideoSourceSlot *)addSource:(id<P1VideoSource>)source withDrawArea:(CGRect)drawArea;
- (void)removeAllSources;

- (NSDictionary *)serialize;
- (void)deserialize:(NSDictionary *)dict;

@end
