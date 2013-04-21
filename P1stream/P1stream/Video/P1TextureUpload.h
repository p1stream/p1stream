#include <gst/base/gstbasetransform.h>


#define P1_TYPE_TEXTURE_UPLOAD \
    (p1_texture_upload_get_type())
#define P1_TEXTURE_UPLOAD(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),  P1_TYPE_TEXTURE_UPLOAD, P1TextureUpload))
#define P1_TEXTURE_UPLOAD_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),   P1_TYPE_TEXTURE_UPLOAD, P1TextureUploadClass))
#define P1_IS_TEXTURE_UPLOAD(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),  P1_TYPE_TEXTURE_UPLOAD))
#define P1_IS_TEXTURE_UPLOAD_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),   P1_TYPE_TEXTURE_UPLOAD))
#define P1_TEXTURE_UPLOAD_GET_CLASS(klass) \
    (G_TYPE_INSTANCE_GET_CLASS((klass), P1_TYPE_TEXTURE_UPLOAD, P1TextureUploadClass))

typedef struct _P1TextureUpload P1TextureUpload;
typedef struct _P1TextureUploadClass P1TextureUploadClass;

struct _P1TextureUpload
{
    GstBaseTransform parent_instance;

    /*< private >*/
    gint width, height;
};

struct _P1TextureUploadClass
{
    GstBaseTransformClass parent_class;
};

GType p1_texture_upload_get_type();
