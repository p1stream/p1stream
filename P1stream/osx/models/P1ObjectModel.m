#import "P1ObjectModel.h"


@implementation P1ObjectModel

- (id)initWithObject:(P1Object *)object
                name:(NSString *)name
{
    self = [super init];
    if (self) {
        _object = object;
        _name = name;

        _object->user_data = (__bridge void *)self;
    }
    return self;
}

+ (id)modelForObject:(P1Object *)object
{
    if (object) {
        void *ptr = object->user_data;
        if (ptr)
            return (__bridge P1ObjectModel *)ptr;
    }
    return nil;
}


- (P1ContextModel *)contextModel
{
    P1Object *ctxobj = (P1Object *)_object->ctx;
    return (__bridge P1ContextModel *)ctxobj->user_data;
}


- (void)lock
{
    p1_object_lock(_object);
}

- (void)unlock
{
    p1_object_unlock(_object);
}


- (void)handleNotification:(P1Notification *)n
{
    [self willChangeValueForKey:@"state"];
    _state = n->state;
    [self didChangeValueForKey:@"state"];
}


- (P1CurrentState)currentState
{
    return _state.current;
}
+ (NSSet *)keyPathsForValuesAffectingCurrentState
{
    return [NSSet setWithObjects:@"state", nil];
}


- (P1TargetState)target
{
    return _state.target;
}
+ (NSSet *)keyPathsForValuesAffectingTarget
{
    return [NSSet setWithObjects:@"state", nil];
}


- (void)setTarget:(P1TargetState)target
{
    [self lock];
    p1_object_target(_object, target);
    [self unlock];
}
+ (BOOL)automaticallyNotifiesObserversForKey:(NSString *)key
{
    if ([key isEqualToString:@"target"])
        return NO;
    else
        return [super automaticallyNotifiesObserversForKey:key];
}


- (BOOL)needsRestart
{
    return (_state.flags & P1_FLAG_NEEDS_RESTART);
}
+ (NSSet *)keyPathsForValuesAffectingNeedsRestart
{
    return [NSSet setWithObjects:@"state", nil];
}


- (BOOL)configValid
{
    return (_state.flags & P1_FLAG_CONFIG_VALID);
}
+ (NSSet *)keyPathsForValuesAffectingConfigValid
{
    return [NSSet setWithObjects:@"state", nil];
}


- (BOOL)canStart
{
    return (_state.flags & P1_FLAG_CAN_START);
}
+ (NSSet *)keyPathsForValuesAffectingCanStart
{
    return [NSSet setWithObjects:@"state", nil];
}


- (BOOL)error
{
    return (_state.flags & P1_FLAG_ERROR);
}
+ (NSSet *)keyPathsForValuesAffectingError
{
    return [NSSet setWithObjects:@"state", nil];
}


- (void)restartIfNeeded
{
    if (self.needsRestart)
        self.target = P1_TARGET_RESTART;
}


- (NSImage *)availabilityImage
{
    NSString *imageName;

    if (self.error) {
        imageName = NSImageNameStatusUnavailable;
    }
    else {
        switch (self.currentState) {
            case P1_STATE_IDLE:
                imageName = NSImageNameStatusNone;
                break;
            case P1_STATE_STARTING:
                imageName = NSImageNameStatusPartiallyAvailable;
                break;
            case P1_STATE_RUNNING:
                imageName = NSImageNameStatusAvailable;
                break;
            case P1_STATE_STOPPING:
                imageName = NSImageNameStatusPartiallyAvailable;
                break;
            default:
                imageName = NSImageNameStatusUnavailable;
                break;
        }
    }

    return [NSImage imageNamed:imageName];
}
+ (NSSet *)keyPathsForValuesAffectingAvailabilityImage
{
    return [NSSet setWithObjects:@"error", @"currentState", nil];
}

@end
