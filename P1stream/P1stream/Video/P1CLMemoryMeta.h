#import "P1CLContext.h"


typedef struct _P1CLMemoryMeta P1CLMemoryMeta;

struct _P1CLMemoryMeta {
    GstMeta meta;

    P1CLContext *context;
    cl_mem ptr;
};

GType p1_cl_memory_meta_api_get_type();
#define P1_FRAME_BUFFER_META_API_TYPE (p1_cl_memory_meta_api_get_type())

const GstMetaInfo *p1_cl_memory_meta_get_info();
#define P1_FRAME_BUFFER_META_INFO (p1_cl_memory_meta_get_info())

#define gst_buffer_get_cl_memory_meta(b) \
    ((P1CLMemoryMeta *)gst_buffer_get_meta((b), P1_FRAME_BUFFER_META_API_TYPE))

#define gst_buffer_add_cl_memory_meta(b, ctx) \
    ((P1CLMemoryMeta *)gst_buffer_add_meta((b), P1_FRAME_BUFFER_META_INFO, ctx))

GstBuffer *gst_buffer_new_cl_memory(P1CLContext *context, cl_mem ptr);
