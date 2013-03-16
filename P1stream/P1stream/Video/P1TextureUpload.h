#include <gst/base/gstbasetransform.h>
#import "P1OpenGLContext.h"


#define P1G_TYPE_TEXTURE_UPLOAD \
    (p1g_texture_upload_get_type())
#define P1G_TEXTURE_UPLOAD(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),  P1G_TYPE_TEXTURE_UPLOAD, P1GTextureUpload))
#define P1G_TEXTURE_UPLOAD_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),   P1G_TYPE_TEXTURE_UPLOAD, P1GTextureUploadClass))
#define P1G_IS_TEXTURE_UPLOAD(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),  P1G_TYPE_TEXTURE_UPLOAD))
#define P1G_IS_TEXTURE_UPLOAD_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),   P1G_TYPE_TEXTURE_UPLOAD))
#define P1G_TEXTURE_UPLOAD_GET_CLASS(klass) \
    (G_TYPE_INSTANCE_GET_CLASS((klass), P1G_TYPE_TEXTURE_UPLOAD, P1GTextureUploadClass))

typedef struct _P1GTextureUpload P1GTextureUpload;
typedef struct _P1GTextureUploadClass P1GTextureUploadClass;

struct _P1GTextureUpload
{
    GstBaseTransform parent_instance;

    /*< private >*/
    gint width, height;
};

struct _P1GTextureUploadClass
{
    GstBaseTransformClass parent_class;
};

GType p1g_texture_upload_get_type();
