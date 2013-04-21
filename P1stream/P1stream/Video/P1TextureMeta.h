#import "P1GLContext.h"


typedef struct _P1TextureMeta P1TextureMeta;

struct _P1TextureMeta {
    GstMeta meta;

    P1GLContext *context;
    GLuint name;

    GstBuffer *dependency;
};

GType p1_texture_meta_api_get_type();
#define P1_TEXTURE_META_API_TYPE (p1_texture_meta_api_get_type())

const GstMetaInfo *p1_texture_meta_get_info();
#define P1_TEXTURE_META_INFO (p1_texture_meta_get_info())

#define gst_buffer_get_texture_meta(b) \
    ((P1TextureMeta *)gst_buffer_get_meta((b), P1_TEXTURE_META_API_TYPE))

#define gst_buffer_add_texture_meta(b, ctx) \
    ((P1TextureMeta *)gst_buffer_add_meta((b), P1_TEXTURE_META_INFO, ctx))

GstBuffer *gst_buffer_new_texture(P1GLContext *context);
