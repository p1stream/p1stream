#import "P1TextureUpload.h"


G_DEFINE_TYPE(P1GTextureUpload, p1g_texture_upload, GST_TYPE_VIDEO_FILTER)
static GstVideoFilterClass *parent_class = NULL;

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE(
    "sink", GST_PAD_SINK, GST_PAD_ALWAYS, GST_STATIC_CAPS(
        "video/x-raw, "
            "format = (string) { BGRA, ABGR, RGBA, ARGB, BGRx, xBGR, RGBx, xRGB }, "
            "width = (int) [ 1, max ], "
            "height = (int) [ 1, max ], "
            "framerate = (fraction) [ 0, max ]"
    )
);

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE(
    "src", GST_PAD_SRC, GST_PAD_ALWAYS, GST_STATIC_CAPS(
        "video/x-gl-texture, "
            "width = (int) [ 1, max ], "
            "height = (int) [ 1, max ], "
            "framerate = (fraction) [ 0, max ]"
    )
);


static void p1g_texture_upload_class_init(P1GTextureUploadClass *klass)
{
    parent_class = g_type_class_ref(GST_TYPE_VIDEO_FILTER);

    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&sink_template));
    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&src_template));
    gst_element_class_set_static_metadata(element_class, "P1stream texture upload",
                                           "Filter/Video",
                                           "Uploads a frame as a texture to an OpenGL context",
                                           "Stéphan Kochen <stephan@kochen.nl>");

}

static void p1g_texture_upload_init(P1GTextureUpload *self)
{
}
