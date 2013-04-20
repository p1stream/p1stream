#import "P1OpenGLContext.h"


typedef struct _P1GTextureMeta P1GTextureMeta;

struct _P1GTextureMeta {
    GstMeta meta;

    P1GOpenGLContext *context;
    GLuint name;

    GstBuffer *dependency;
};

GType p1g_texture_meta_api_get_type();
#define P1G_TEXTURE_META_API_TYPE (p1g_texture_meta_api_get_type())

const GstMetaInfo *p1g_texture_meta_get_info();
#define P1G_TEXTURE_META_INFO (p1g_texture_meta_get_info())

#define gst_buffer_get_texture_meta(b) \
    ((P1GTextureMeta *)gst_buffer_get_meta((b), P1G_TEXTURE_META_API_TYPE))

#define gst_buffer_add_texture_meta(b, ctx) \
    ((P1GTextureMeta *)gst_buffer_add_meta((b), P1G_TEXTURE_META_INFO, ctx))

GstBuffer *gst_buffer_new_texture(P1GOpenGLContext *context);
