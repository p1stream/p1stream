#include <gst/base/gstcollectpads.h>
#include "P1TexturePool.h"


#define P1G_TYPE_RENDER_TEXTURES \
    (p1g_render_textures_get_type())
#define P1G_RENDER_TEXTURES(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),  P1G_TYPE_RENDER_TEXTURES, P1GRenderTextures))
#define P1G_RENDER_TEXTURES_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),   P1G_TYPE_RENDER_TEXTURES, P1GRenderTexturesClass))
#define P1G_IS_RENDER_TEXTURES(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),  P1G_TYPE_RENDER_TEXTURES))
#define P1G_IS_RENDER_TEXTURES_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),   P1G_TYPE_RENDER_TEXTURES))
#define P1G_RENDER_TEXTURES_GET_CLASS(klass) \
    (G_TYPE_INSTANCE_GET_CLASS((klass), P1G_TYPE_RENDER_TEXTURES, P1GRenderTexturesClass))

typedef struct _P1GRenderTextures P1GRenderTextures;
typedef struct _P1GRenderTexturesClass P1GRenderTexturesClass;

struct _P1GRenderTextures
{
    GstElement parent_instance;

    /*< private >*/
    GstPad *src;
    GstCollectPads *collect;

    P1GTexturePool *pool;
    P1GOpenGLContext *context;
    GLuint vao_name;
    GLuint vbo_name;
    GLuint program_name;
    GLuint texture_uniform;

    gint width;
    gint height;

    gboolean send_stream_start;
    gboolean send_caps;
};

struct _P1GRenderTexturesClass
{
    GstElementClass parent_class;
};

GType p1g_render_textures_get_type();
