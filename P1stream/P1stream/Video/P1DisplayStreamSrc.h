#include <gst/base/gstpushsrc.h>


#define P1G_TYPE_DISPLAY_STREAM_SRC \
    (p1g_display_stream_src_get_type())
#define P1G_DISPLAY_STREAM_SRC(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),  P1G_TYPE_DISPLAY_STREAM_SRC, P1GDisplayStreamSrc))
#define P1G_DISPLAY_STREAM_SRC_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),   P1G_TYPE_DISPLAY_STREAM_SRC, P1GDisplayStreamSrcClass))
#define P1G_IS_DISPLAY_STREAM_SRC(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),  P1G_TYPE_DISPLAY_STREAM_SRC))
#define P1G_IS_DISPLAY_STREAM_SRC_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),   P1G_TYPE_DISPLAY_STREAM_SRC))
#define P1G_DISPLAY_STREAM_SRC_GET_CLASS(klass) \
    (G_TYPE_INSTANCE_GET_CLASS((klass), P1G_TYPE_DISPLAY_STREAM_SRC, P1GDisplayStreamSrcClass))

typedef struct _P1GDisplayStreamSrc P1GDisplayStreamSrc;
typedef struct _P1GDisplayStreamSrcClass P1GDisplayStreamSrcClass;

struct _P1GDisplayStreamSrc
{
    GstPushSrc parent_instance;

    /*< private >*/
    CGDisplayStreamRef display_stream;
    GstBuffer *buffer;
    gboolean flushing;
    gboolean stopped;
    GCond cond;
};

struct _P1GDisplayStreamSrcClass
{
    GstPushSrcClass parent_class;
};

GType p1g_display_stream_src_get_type();
