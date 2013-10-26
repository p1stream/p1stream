#import "P1PluginModel.h"


@implementation P1PluginModel

- (id)initWithObject:(P1Object *)object
                name:(NSString *)name
                uuid:(NSString *)uuid
{
    self = [super initWithObject:object name:name];
    if (self) {
        _uuid = uuid;

        // Hold a strong reference.
        CFRetain(object->user_data);
    }
    return self;
}

- (void)remove
{
    [self willChangeValueForKey:@"isPendingRemoval"];
    _isPendingRemoval = TRUE;
    [self didChangeValueForKey:@"isPendingRemoval"];

    self.target = P1_TARGET_IDLE;
}

- (void)destroy
{
    P1Object *object = self.object;
    P1Plugin *plugin = (P1Plugin *)object;

    CFRelease(object->user_data);
    p1_plugin_free(plugin);
}

@end
