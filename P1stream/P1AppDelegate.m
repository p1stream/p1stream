#import "P1AppDelegate.h"
#import "P1GCALayerSink.h"


@implementation P1AppDelegate

static gboolean p1g_register_elements(GstPlugin *plugin)
{
    gboolean ok;

    ok = gst_element_register(plugin, "calayersink", GST_RANK_NONE, P1G_TYPE_CALAYER_SINK);

    return ok;
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    gst_bundle_init();

    gst_plugin_register_static(GST_VERSION_MAJOR, GST_VERSION_MINOR,
                               "p1gelements", "P1stream elements", p1g_register_elements,
                               "0.1", "Proprietary", "P1stream", "P1stream", "P1stream");
}

@end
