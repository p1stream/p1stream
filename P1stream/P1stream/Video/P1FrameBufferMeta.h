#import "P1GLContext.h"


typedef struct _P1FrameBufferMeta P1FrameBufferMeta;

struct _P1FrameBufferMeta {
    GstMeta meta;

    P1GLContext *context;
    GLuint name;
};

GType p1_frame_buffer_meta_api_get_type();
#define P1_FRAME_BUFFER_META_API_TYPE (p1_frame_buffer_meta_api_get_type())

const GstMetaInfo *p1_frame_buffer_meta_get_info();
#define P1_FRAME_BUFFER_META_INFO (p1_frame_buffer_meta_get_info())

#define gst_buffer_get_frame_buffer_meta(b) \
    ((P1FrameBufferMeta *)gst_buffer_get_meta((b), P1_FRAME_BUFFER_META_API_TYPE))

#define gst_buffer_add_frame_buffer_meta(b, ctx) \
    ((P1FrameBufferMeta *)gst_buffer_add_meta((b), P1_FRAME_BUFFER_META_INFO, ctx))

GstBuffer *gst_buffer_new_frame_buffer(P1GLContext *context);
