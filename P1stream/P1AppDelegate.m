#import "P1AppDelegate.h"


@implementation P1AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    // Insurance, because an env variable can override it.
    gst_registry_fork_set_enabled(false);

    // This can't fail with GST_DISABLE_PARSE.
    gst_init(0, NULL);

    // Load core plugins from our bundle.
    NSString *coreElementsPath = [@[
            [[NSBundle mainBundle] bundlePath],
            @"Contents", @"Frameworks", @"libgstcoreelementss.dylib"
        ] componentsJoinedByString:@"/"];
    GError *err = NULL;
    gst_plugin_load_file([coreElementsPath UTF8String], &err);
    if (err) {
        [NSException raise:NSGenericException
                    format:@"Failed to load GStreamer core elements plugin: %@",
                           [NSString stringWithUTF8String:err->message]
         ];
    }
}

@end
