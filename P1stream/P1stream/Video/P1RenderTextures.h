#include <gst/base/gstcollectpads.h>
#include "P1TexturePool.h"


#define P1_TYPE_RENDER_TEXTURES \
    (p1_render_textures_get_type())
#define P1_RENDER_TEXTURES(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),  P1_TYPE_RENDER_TEXTURES, P1RenderTextures))
#define P1_RENDER_TEXTURES_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),   P1_TYPE_RENDER_TEXTURES, P1RenderTexturesClass))
#define P1_IS_RENDER_TEXTURES(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),  P1_TYPE_RENDER_TEXTURES))
#define P1_IS_RENDER_TEXTURES_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),   P1_TYPE_RENDER_TEXTURES))
#define P1_RENDER_TEXTURES_GET_CLASS(klass) \
    (G_TYPE_INSTANCE_GET_CLASS((klass), P1_TYPE_RENDER_TEXTURES, P1RenderTexturesClass))

typedef struct _P1RenderTextures P1RenderTextures;
typedef struct _P1RenderTexturesClass P1RenderTexturesClass;

struct _P1RenderTextures
{
    GstElement parent_instance;

    /*< private >*/
    GstPad *src;

    P1GLContext *context;
    GLuint vao_name;
    GLuint vbo_name;
    GLuint program_name;
    GLuint texture_uniform;

    GstBufferPool *pool;
    GstCollectPads *collect;

    gint width;
    gint height;

    gboolean send_stream_start;
    gboolean send_caps;
};

struct _P1RenderTexturesClass
{
    GstElementClass parent_class;
};

GType p1_render_textures_get_type();
