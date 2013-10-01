@interface P1ObjectModel : NSObject

@property (readonly) P1Object *object;

@property (readonly) P1State state;
@property P1TargetState target;

@property (retain) NSString *name;

- (id)initWithObject:(P1Object *)object;

- (void)lock;
- (void)unlock;

- (void)handleNotification:(P1Notification *)n;

- (void)clearHalt;

@end
