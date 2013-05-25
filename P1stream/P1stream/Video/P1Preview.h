#include <gst/video/gstvideosink.h>

@class P1Preview;


#define P1_TYPE_PREVIEW_SINK \
    (p1_preview_sink_get_type())
#define P1_PREVIEW_SINK(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),  P1_TYPE_PREVIEW_SINK, P1PreviewSink))
#define P1_PREVIEW_SINK_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),   P1_TYPE_PREVIEW_SINK, P1PreviewSinkClass))
#define P1_IS_PREVIEW_SINK(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),  P1_TYPE_PREVIEW_SINK))
#define P1_IS_PREVIEW_SINK_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),   P1_TYPE_PREVIEW_SINK))
#define P1_PREVIEW_SINK_GET_CLASS(klass) \
    (G_TYPE_INSTANCE_GET_CLASS((klass), P1_TYPE_PREVIEW_SINK, P1PreviewSinkClass))

typedef struct _P1PreviewSink P1PreviewSink;
typedef struct _P1PreviewSinkClass P1PreviewSinkClass;

struct _P1PreviewSink
{
    GstVideoSink parent_instance;

    CFTypeRef viewRef;
};

struct _P1PreviewSinkClass
{
    GstVideoSinkClass parent_class;
};

GType p1_preview_sink_get_type();


@interface P1Preview : NSOpenGLView
{
    P1GLContext *gobjContext;

    GstBuffer *currentBuffer;
    NSLayoutConstraint *videoConstraint;

    GLuint shaderProgram;
    GLint textureUniform;
    GLuint vertexArrayName;
    GLuint vertexBufferName;
}

@property (nonatomic, readonly) P1PreviewSink *element;
@property (nonatomic) CGFloat aspect;

- (CGLContextObj)CGLContextObj;
- (P1GLContext *)gobjContext;

- (void)updateVideoConstraint;
- (void)clearVideoConstraint;

- (void)setBuffer:(GstBuffer *)buffer;

@end
