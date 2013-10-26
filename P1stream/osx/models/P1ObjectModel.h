@class P1ContextModel;


@interface P1ObjectModel : NSObject

@property (readonly) P1Object *object;
@property (readonly) P1ContextModel *contextModel;
@property (readonly) P1State state;

@property (readonly) P1CurrentState currentState;
@property P1TargetState target;
@property (readonly) BOOL needsRestart;
@property (readonly) BOOL configValid;
@property (readonly) BOOL canStart;
@property (readonly) BOOL error;

@property (readonly) NSImage *availabilityImage;

@property (retain) NSString *name;

- (id)initWithObject:(P1Object *)object
                name:(NSString *)name;
+ (id)modelForObject:(P1Object *)object;

- (void)lock;
- (void)unlock;

- (void)handleNotification:(P1Notification *)n;

- (void)restartIfNeeded;

+ (NSSet *)keyPathsForValuesAffectingCurrentState;
+ (NSSet *)keyPathsForValuesAffectingTarget;
+ (NSSet *)keyPathsForValuesAffectingNeedsRestart;
+ (NSSet *)keyPathsForValuesAffectingConfigValid;
+ (NSSet *)keyPathsForValuesAffectingCanStart;
+ (NSSet *)keyPathsForValuesAffectingError;
+ (NSSet *)keyPathsForValuesAffectingAvailabilityImage;

@end
