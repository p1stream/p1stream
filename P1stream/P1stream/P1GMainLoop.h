@interface P1GMainLoop : NSObject
{
    pthread_t thread;
}

@property (assign, nonatomic, readonly) GMainLoop *gMainLoop;

+ (P1GMainLoop *)defaultMainLoop;

- (void)start;
- (void)stop;

@end


@interface NSObject (P1GObjectWithGMainLoop)

- (void)performSelectorOnGMainLoop:(SEL)aSelector withObject:(id)arg;

@end
