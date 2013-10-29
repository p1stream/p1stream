#import "P1VideoModel.h"
#import "P1ContextModel.h"


@implementation P1VideoModel

- (id)initWithContext:(P1Context *)context
{
    return [super initWithObject:(P1Object *)context->video name:@"Video mixer"];
}

- (P1PluginModel *)clockModel
{
    P1Video *video = (P1Video *)self.object;
    return [P1ObjectModel modelForObject:(P1Object *)video->clock];
}

- (void)enumerateSourceModels:(void (^)(P1PluginModel *sourceModel))block
{
    P1Video *video = (P1Video *)self.object;

    [self lock];

    P1ListNode *head = &video->sources;
    P1ListNode *node;
    P1ListNode *next;
    p1_list_iterate_for_removal(head, node, next) {
        P1Source *source = p1_list_get_container(node, P1Source, link);
        P1PluginModel *model = [P1ObjectModel modelForObject:(P1Object *)source];
        if (model)
            block(model);
    }

    [self unlock];
}


- (void)reconfigurePlugins
{
    [self lock];

    [self reconfigureClock];
    [self reconfigureSources];

    p1_video_resync((P1Video *)self.object);

    [self unlock];
}

- (void)reconfigureClock
{
    P1Video *video = (P1Video *)self.object;
    NSUserDefaults *ud = [NSUserDefaults standardUserDefaults];
    NSDictionary *dict = [ud dictionaryForKey:@"video-clock"];
    P1VideoClock *videoClock;

    P1PluginModel *existing = self.clockModel;
    if (existing && existing.uuid == dict[@"uuid"]) {
        // Simply reconfigure the existing clock.
        videoClock = (P1VideoClock *)existing.object;
    }
    else {
        // Determine the factory function to use.
        P1VideoClockFactory *factory = NULL;
        NSString *type = dict[@"type"];
        if ([type isEqualToString:@"display"])
            factory = p1_display_video_clock_create;
        if (factory == NULL)
            return;

        // Create the clock.
        videoClock = factory(self.contextModel.context);
        if (videoClock == NULL)
            return;

        // Create a model. Its initializer hooks into user data.
        P1PluginModel *model = [[P1PluginModel alloc] initWithObject:(P1Object *)videoClock
                                                                name:@"Video clock"
                                                                uuid:dict[@"uuid"]];

        // Set the new clock.
        if (existing) {
            if (_newClock)
                [_newClock destroy];
            _newClock = model;
        }
        else {
            video->clock = videoClock;
        }
    }

    // Configure the clock.
    P1Config *config = p1_plist_config_create(dict);
    if (config != NULL) {
        p1_video_clock_config(videoClock, config);
        p1_config_free(config);
    }
}

- (void)reconfigureSources
{
    P1Video *video = (P1Video *)self.object;
    NSUserDefaults *ud = [NSUserDefaults standardUserDefaults];
    NSArray *arr = [ud arrayForKey:@"video-sources"];

    // Build a map of existing sources by UUID.
    NSMutableDictionary *existing = [NSMutableDictionary new];
    [self enumerateSourceModels:^(P1PluginModel *model) {
        existing[model.uuid] = model;
    }];

    // Iterate configuration, and create sources.
    for (NSDictionary *dict in arr) {
        P1Source *source;
        P1PluginModel *model;

        NSString *uuid = dict[@"uuid"];
        if ((model = existing[uuid])) {
            // Reappend the existing source.
            source = (P1Source *)model.object;
            p1_list_remove(&source->link);

            [existing removeObjectForKey:uuid];
        }
        else {
            // Determine the factory function to use.
            P1VideoSourceFactory *factory = NULL;
            NSString *type = dict[@"type"];
            if ([type isEqualToString:@"display"])
                factory = p1_display_video_source_create;
            else if ([type isEqualToString:@"capture"])
                factory = p1_capture_video_source_create;
            else
                continue;

            // Create the source.
            source = (P1Source *)factory(self.contextModel.context);
            if (source == NULL)
                continue;

            // Create a model. Its initializer hooks into user data.
            model = [[P1PluginModel alloc] initWithObject:(P1Object *)source
                                                     name:dict[@"name"]
                                                     uuid:uuid];
        }

        [model lock];

        // Append the source.
        p1_list_before(&video->sources, &source->link);

        // Configure the source.
        P1Config *config = p1_plist_config_create(dict);
        if (config != NULL) {
            p1_video_source_config((P1VideoSource *)source, config);
            p1_config_free(config);
        }

        [model unlock];
    }

    // Remove old sources.
    for (NSString *uuid in existing)
        [existing[uuid] remove];
}


- (void)handleNotification:(P1Notification *)n
{
    [super handleNotification:n];

    [self enumerateSourceModels:^(P1PluginModel *model) {
        // Check if we need to remove this source.
        if (model.isPendingRemoval && model.currentState == P1_STATE_IDLE) {
            P1Source *source = (P1Source *)model.object;
            p1_list_remove(&source->link);
            [model destroy];
            return;
        }

        // Check if we need to restart this source.
        [model restartIfNeeded];
    }];
}


- (BOOL)hasNewClockPending
{
    return _newClock != NULL;
}

- (void)swapNewClock
{
    P1Video *video = (P1Video *)self.object;

    [self lock];

    [self.clockModel destroy];
    video->clock = (P1VideoClock *)_newClock.object;

    p1_video_resync((P1Video *)self.object);

    [self unlock];
}

@end
