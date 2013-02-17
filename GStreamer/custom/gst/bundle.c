#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "bundle.h"
#include "gst/videotestsrc/gstvideotestsrc.h"


static gboolean
gst_bundle_register_elements (GstPlugin * plugin)
{
    gboolean ok;

    ok = gst_element_register (plugin, "videotestsrc", GST_RANK_NONE,
        GST_TYPE_VIDEO_TEST_SRC);

    return ok;
}

void
gst_bundle_init ()
{
  gst_init (0, NULL);

  gst_plugin_register_static (GST_VERSION_MAJOR, GST_VERSION_MINOR,
      "bundledelements", "elements linked into the GStreamer bundle",
      gst_bundle_register_elements, VERSION, GST_LICENSE, PACKAGE,
      GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN);
}
