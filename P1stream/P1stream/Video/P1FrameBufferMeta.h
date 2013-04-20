#import "P1OpenGLContext.h"


typedef struct _P1GFrameBufferMeta P1GFrameBufferMeta;

struct _P1GFrameBufferMeta {
    GstMeta meta;

    P1GOpenGLContext *context;
    GLuint name;
};

GType p1g_frame_buffer_meta_api_get_type();
#define P1G_FRAME_BUFFER_META_API_TYPE (p1g_frame_buffer_meta_api_get_type())

const GstMetaInfo *p1g_frame_buffer_meta_get_info();
#define P1G_FRAME_BUFFER_META_INFO (p1g_frame_buffer_meta_get_info())

#define gst_buffer_get_frame_buffer_meta(b) \
    ((P1GFrameBufferMeta *)gst_buffer_get_meta((b), P1G_FRAME_BUFFER_META_API_TYPE))

#define gst_buffer_add_frame_buffer_meta(b, ctx) \
    ((P1GFrameBufferMeta *)gst_buffer_add_meta((b), P1G_FRAME_BUFFER_META_INFO, ctx))

GstBuffer *gst_buffer_new_frame_buffer(P1GOpenGLContext *context);
