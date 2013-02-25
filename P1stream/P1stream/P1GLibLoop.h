@interface P1GLibLoop : NSObject
{
    pthread_t thread;
}

@property (assign, nonatomic, readonly) GMainLoop *gMainLoop;

+ (P1GLibLoop *)defaultMainLoop;

- (void)start;
- (void)stop;

@end


@interface NSObject (P1ObjectWithGLibLoop)

- (void)performSelectorOnGMainLoop:(SEL)aSelector withObject:(id)arg;

@end
