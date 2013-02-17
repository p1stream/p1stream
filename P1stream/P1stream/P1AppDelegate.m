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
    [self setupGStreamer];
}

- (NSString *)pathForGStreamerPlugin:(NSString *)pluginName
{
    return [NSString stringWithFormat:@"%@/Contents/GStreamerPlugins/libgst%@.dylib",
            [[NSBundle mainBundle] bundlePath], pluginName];
}

- (GstPlugin *)loadGStreamerPlugin:(NSString *)pluginName
{
    NSString *path = [self pathForGStreamerPlugin:pluginName];
    const char *cPath = [path UTF8String];

    GError *gErr = NULL;
    GstPlugin *plugin = gst_plugin_load_file(cPath, &gErr);
    if (!plugin) {
        NSError *err = [NSError errorWithGError:gErr];
        g_error_free(gErr);
        [[NSAlert alertWithError:err] runModal];
        abort();
    }

    return plugin;
}

- (void)setupGStreamer
{
    gst_init(0, NULL);

    [self loadGStreamerPlugin:@"coreelements"];
    [self loadGStreamerPlugin:@"videotestsrc"];
    [self loadGStreamerPlugin:@"videoconvert"];

    gst_plugin_register_static(GST_VERSION_MAJOR, GST_VERSION_MINOR,
                               "p1gelements", "P1stream elements", p1g_register_elements,
                               "0.1", "Proprietary", "P1stream", "P1stream", "P1stream");
}

@end
