#import "P1GLibLoop.h"


@implementation P1GLibLoop

@synthesize gMainLoop;

+ (P1GLibLoop *)defaultMainLoop
{
    static P1GLibLoop *instance = NULL;
    if (!instance)
        instance = [[P1GLibLoop alloc] init];
    return instance;
}

- (id)init
{
    self = [super init];
    if (self) {
        // Trigger Cocoa threading awareness.
        [self performSelectorInBackground:@selector(dummy) withObject:nil];

        gMainLoop = g_main_loop_new(NULL, FALSE);
    }
    return self;
}

- (void)dealloc
{
    if (gMainLoop) {
        [self stop];
        g_main_loop_unref(gMainLoop);
    }
}

- (void)dummy
{
    // Dummy method spawned in a background thread.
}

static void *mainLoopThread(void *arg)
{
    P1GLibLoop *self = CFBridgingRelease(arg);
    g_main_loop_run(self->gMainLoop);
    return NULL;
}

- (void)start
{
    if (!g_main_loop_is_running(gMainLoop)) {
        void *arg = (void *)CFBridgingRetain(self);
        pthread_create(&thread, NULL, mainLoopThread, arg);
    }
}

- (void)stop
{
    if (g_main_loop_is_running(gMainLoop)) {
        g_main_loop_quit(gMainLoop);
        pthread_join(thread, NULL);
    }
}

@end


@implementation NSObject (P1ObjectWithGLibLoop)

struct IdleCallbackData
{
    CFTypeRef self;
    SEL selector;
    CFTypeRef arg;
};

static gboolean idleCallback(gpointer userData)
{
    struct IdleCallbackData *data = userData;
    id self = CFBridgingRelease(data->self);
    id arg = CFBridgingRelease(data->arg);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
    [self performSelector:data->selector withObject:arg];
#pragma clang diagnostic pop

    g_free(data);
    return FALSE;
}

- (void)performSelectorOnGMainLoop:(SEL)selector withObject:(id)arg
{
    struct IdleCallbackData *data = g_malloc(sizeof(struct IdleCallbackData));
    data->self = CFBridgingRetain(self);
    data->selector = selector;
    data->arg = CFBridgingRetain(arg);
    g_idle_add_full(G_PRIORITY_DEFAULT, idleCallback, data, NULL);
}

@end
