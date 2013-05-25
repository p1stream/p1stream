#include <gst/base/gstbasetransform.h>


#define P1_TYPE_TEXTURE_DOWNLOAD \
    (p1_texture_download_get_type())
#define P1_TEXTURE_DOWNLOAD(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),  P1_TYPE_TEXTURE_DOWNLOAD, P1TextureDownload))
#define P1_TEXTURE_DOWNLOAD_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),   P1_TYPE_TEXTURE_DOWNLOAD, P1TextureDownloadClass))
#define P1_IS_TEXTURE_DOWNLOAD(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),  P1_TYPE_TEXTURE_DOWNLOAD))
#define P1_IS_TEXTURE_DOWNLOAD_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),   P1_TYPE_TEXTURE_DOWNLOAD))
#define P1_TEXTURE_DOWNLOAD_GET_CLASS(klass) \
    (G_TYPE_INSTANCE_GET_CLASS((klass), P1_TYPE_TEXTURE_DOWNLOAD, P1TextureDownloadClass))

typedef struct _P1TextureDownload P1TextureDownload;
typedef struct _P1TextureDownloadClass P1TextureDownloadClass;

struct _P1TextureDownload
{
    GstBaseTransform parent_instance;

    /*< private >*/
    P1GLContext *context, *download_context;
};

struct _P1TextureDownloadClass
{
    GstBaseTransformClass parent_class;
};

GType p1_texture_download_get_type();
