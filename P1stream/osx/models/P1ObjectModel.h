@interface P1ObjectModel : NSObject
{
    BOOL _restart;
}

@property (readonly) P1Object *object;

@property (readonly) P1State state;
@property P1TargetState target;
@property (readonly) BOOL error;

@property (retain) NSString *name;

@property (readonly) NSImage *availabilityImage;

- (id)initWithObject:(P1Object *)object;

- (void)lock;
- (void)unlock;

- (void)restart;

- (void)handleNotification:(P1Notification *)n;

+ (NSSet *)keyPathsForValuesAffectingAvailabilityImage;

@end
