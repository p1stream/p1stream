#import "P1Utils.h"
#import "P1IOSurfaceBuffer.h"
#import "P1TextureUpload.h"
#import "P1TextureDownload.h"
#import "P1RenderTextures.h"
#import "P1Preview.h"
#import "P1DisplayStreamSrc.h"


static void gstreamerLogCallback(GstDebugCategory *category, GstDebugLevel level,
                                 const gchar *file, const gchar *function, gint line,
                                 GObject *object, GstDebugMessage *message, gpointer unused)
{
    NSLog(@"%s %20s: %s",
          gst_debug_level_get_name(level),
          gst_debug_category_get_name(category),
          gst_debug_message_get(message));
}

static void loadGStreamerPlugin(NSString *pluginName)
{
    NSString *path = [NSString stringWithFormat:@"%@/Contents/GStreamerPlugins/libgst%@.dylib",
                      [[NSBundle mainBundle] bundlePath], pluginName];
    const char *cPath = [path UTF8String];

    GError *gErr = NULL;
    GstPlugin *plugin = gst_plugin_load_file(cPath, &gErr);
    if (!plugin) {
        NSString *message = [NSString stringWithUTF8String:gErr->message];
        g_error_free(gErr);
        [NSException raise:NSGenericException
                    format:@"Unable to load '%@' GStreamer plugin: %@",
                           pluginName, message];
    }
    g_object_unref(plugin);
}

static gboolean registerGStreamerElements(GstPlugin *plugin)
{
    gboolean ok = TRUE;

    ok = ok && gst_element_register(plugin, "textureupload",    GST_RANK_NONE, P1_TYPE_TEXTURE_UPLOAD);
    ok = ok && gst_element_register(plugin, "texturedownload",  GST_RANK_NONE, P1_TYPE_TEXTURE_DOWNLOAD);
    ok = ok && gst_element_register(plugin, "rendertextures",   GST_RANK_NONE, P1_TYPE_RENDER_TEXTURES);
    ok = ok && gst_element_register(plugin, "previewsink",      GST_RANK_NONE, P1_TYPE_PREVIEW_SINK);
    ok = ok && gst_element_register(plugin, "displaystreamsrc", GST_RANK_NONE, P1_TYPE_DISPLAY_STREAM_SRC);

    return ok;
}

int main(int argc, char *argv[])
{
    gst_init(argc, &argv);

    gst_debug_remove_log_function(gst_debug_log_default);
    gst_debug_add_log_function(gstreamerLogCallback, NULL, NULL);

    loadGStreamerPlugin(@"coreelements");
    loadGStreamerPlugin(@"videotestsrc");
    loadGStreamerPlugin(@"videoconvert");
    loadGStreamerPlugin(@"x264enc");

    p1_utils_static_init();
    p1_iosurface_allocator_static_init();

    gboolean res = gst_plugin_register_static(
        GST_VERSION_MAJOR, GST_VERSION_MINOR,
        "p1gelements", "P1stream elements", registerGStreamerElements,
        "0.1", "Proprietary", "P1stream", "P1stream", "P1stream");
    if (!res) {
        [NSException raise:NSGenericException
                    format:@"Unable to register GStreamer elements"];
    }

    return NSApplicationMain(argc, (const char **)argv);
}
