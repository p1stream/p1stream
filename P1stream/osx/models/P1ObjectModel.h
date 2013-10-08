@interface P1ObjectModel : NSObject

@property (readonly) P1Object *object;

@property (readonly) P1State state;
@property P1TargetState target;
@property (readonly) BOOL error;

@property (retain) NSString *name;

@property (readonly) NSImage *availabilityImage;

- (id)initWithObject:(P1Object *)object;

- (void)lock;
- (void)unlock;

- (void)handleNotification:(P1Notification *)n;

+ (NSSet *)keyPathsForValuesAffectingAvailabilityImage;

@end
