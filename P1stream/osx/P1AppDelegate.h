@interface P1AppDelegate : NSObject <NSApplicationDelegate>
{
    bool terminating;

    P1Context *context;
    NSFileHandle *contextFileHandle;
}

@end
