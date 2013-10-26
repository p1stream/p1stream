#import "P1AudioModel.h"
#import "P1ContextModel.h"


@implementation P1AudioModel

- (id)initWithContext:(P1Context *)context
{
    return [super initWithObject:(P1Object *)context->audio name:@"Audio mixer"];
}


- (void)enumerateSourceModels:(void (^)(P1PluginModel *sourceModel))block
{
    P1Audio *audio = (P1Audio *)self.object;

    [self lock];

    P1ListNode *head = &audio->sources;
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

    [self reconfigureSources];

    p1_audio_resync((P1Audio *)self.object);

    [self unlock];
}

- (void)reconfigureSources
{
    P1Audio *audio = (P1Audio *)self.object;
    NSUserDefaults *ud = [NSUserDefaults standardUserDefaults];
    NSArray *arr = [ud arrayForKey:@"audio-sources"];

    // Build a map of existing sources by UUID.
    NSMutableDictionary *existing = [NSMutableDictionary new];
    [self enumerateSourceModels:^(P1PluginModel *model) {
        existing[model.uuid] = model;
    }];

    // Iterate configuration, and create sources.
    for (NSDictionary *dict in arr) {
        P1Source *source;
        P1ObjectModel *model;

        NSString *uuid = dict[@"uuid"];
        if ((model = existing[uuid])) {
            // Reappend the existing source.
            source = (P1Source *)model.object;
            p1_list_remove(&source->link);

            [existing removeObjectForKey:uuid];
        }
        else {
            // Determine the factory function to use.
            P1AudioSourceFactory *factory = NULL;
            NSString *type = dict[@"type"];
            if ([type isEqualToString:@"input"])
                factory = p1_input_audio_source_create;
            else
                continue;

            // Create the source.
            source = (P1Source *)factory(self.contextModel.context);
            if (source == NULL)
                continue;

            // Create a model. Its initializer hooks into user data.
            model = [[P1PluginModel alloc] initWithObject:(P1Object *)source
                                                     name:dict[@"name"]
                                                     uuid:dict[@"uuid"]];
        }

        [model lock];

        // Append the source.
        p1_list_before(&audio->sources, &source->link);

        // Configure the source.
        P1Config *config = p1_plist_config_create(dict);
        if (config != NULL) {
            p1_audio_source_config((P1AudioSource *)source, config);
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
        [self restartIfNeeded];
    }];
}

@end
