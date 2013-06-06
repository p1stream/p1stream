#include <gst/base/gstbasetransform.h>
#import "P1CLContext.h"


#define P1_TYPE_CL_MEMORY_UPLOAD \
    (p1_cl_memory_upload_get_type())
#define P1_CL_MEMORY_UPLOAD(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),  P1_TYPE_CL_MEMORY_UPLOAD, P1CLMemoryUpload))
#define P1_CL_MEMORY_UPLOAD_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),   P1_TYPE_CL_MEMORY_UPLOAD, P1CLMemoryUploadClass))
#define P1_IS_CL_MEMORY_UPLOAD(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),  P1_TYPE_CL_MEMORY_UPLOAD))
#define P1_IS_CL_MEMORY_UPLOAD_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),   P1_TYPE_CL_MEMORY_UPLOAD))
#define P1_CL_MEMORY_UPLOAD_GET_CLASS(klass) \
    (G_TYPE_INSTANCE_GET_CLASS((klass), P1_TYPE_CL_MEMORY_UPLOAD, P1CLMemoryUploadClass))

typedef struct _P1CLMemoryUpload P1CLMemoryUpload;
typedef struct _P1CLMemoryUploadClass P1CLMemoryUploadClass;

struct _P1CLMemoryUpload
{
    GstBaseTransform parent_instance;

    P1CLContext *context;
};

struct _P1CLMemoryUploadClass
{
    GstBaseTransformClass parent_class;
};

GType p1_cl_memory_upload_get_type();
