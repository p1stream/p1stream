#include <gst/base/gstpushsrc.h>


#define P1_TYPE_DISPLAY_STREAM_SRC \
    (p1_display_stream_src_get_type())
#define P1_DISPLAY_STREAM_SRC(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),  P1_TYPE_DISPLAY_STREAM_SRC, P1DisplayStreamSrc))
#define P1_DISPLAY_STREAM_SRC_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),   P1_TYPE_DISPLAY_STREAM_SRC, P1DisplayStreamSrcClass))
#define P1_IS_DISPLAY_STREAM_SRC(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),  P1_TYPE_DISPLAY_STREAM_SRC))
#define P1_IS_DISPLAY_STREAM_SRC_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),   P1_TYPE_DISPLAY_STREAM_SRC))
#define P1_DISPLAY_STREAM_SRC_GET_CLASS(klass) \
    (G_TYPE_INSTANCE_GET_CLASS((klass), P1_TYPE_DISPLAY_STREAM_SRC, P1DisplayStreamSrcClass))

typedef struct _P1DisplayStreamSrc P1DisplayStreamSrc;
typedef struct _P1DisplayStreamSrcClass P1DisplayStreamSrcClass;

struct _P1DisplayStreamSrc
{
    GstPushSrc parent_instance;

    /*< private >*/
    CGDisplayStreamRef display_stream;
    GstBuffer *buffer;
    gboolean flushing;
    gboolean stopped;
    GCond cond;
};

struct _P1DisplayStreamSrcClass
{
    GstPushSrcClass parent_class;
};

GType p1_display_stream_src_get_type();
