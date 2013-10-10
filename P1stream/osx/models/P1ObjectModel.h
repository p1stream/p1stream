@interface P1ObjectModel : NSObject
{
    P1State _state;
}

@property (readonly) P1Object *object;

@property (readonly) P1State state;

@property (readonly) P1CurrentState currentState;
@property P1TargetState target;
@property (readonly) BOOL error;

@property (retain) NSString *name;

@property (readonly) NSImage *availabilityImage;

- (id)initWithObject:(P1Object *)object;

- (void)lock;
- (void)unlock;

- (void)handleNotification:(P1Notification *)n;

+ (NSSet *)keyPathsForValuesAffectingCurrentState;
+ (NSSet *)keyPathsForValuesAffectingTarget;
+ (NSSet *)keyPathsForValuesAffectingError;
+ (NSSet *)keyPathsForValuesAffectingAvailabilityImage;

@end
