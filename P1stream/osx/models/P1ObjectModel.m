#import "P1ObjectModel.h"


@implementation P1ObjectModel

- (id)initWithObject:(P1Object *)object
{
    self = [super init];
    if (self) {
        _object = object;
        _object->user_data = (__bridge void *)self;

        switch (_object->type) {
            case P1_OTYPE_CONTEXT:
                _name = @"Context";
                break;
            case P1_OTYPE_AUDIO:
                _name = @"Audio mixer";
                break;
            case P1_OTYPE_VIDEO:
                _name = @"Video mixer";
                break;
            case P1_OTYPE_CONNECTION:
                _name = @"Connection";
                break;
            case P1_OTYPE_AUDIO_SOURCE:
                _name = [NSString stringWithFormat:@"Audio source %p", _object];
                break;
            case P1_OTYPE_VIDEO_CLOCK:
                _name = @"Video clock";
                break;
            case P1_OTYPE_VIDEO_SOURCE:
                _name = [NSString stringWithFormat:@"Video source %p", _object];
                break;
        }
    }
    return self;
}


- (void)lock
{
    p1_object_lock(_object);
}

- (void)unlock
{
    p1_object_unlock(_object);
}


// We handle these using notifications.
+ (BOOL)automaticallyNotifiesObserversForKey:(NSString *)key
{
    if ([key isEqualToString:@"state"])
        return NO;
    else if ([key isEqualToString:@"currentState"])
        return NO;
    else if ([key isEqualToString:@"target"])
        return NO;
    else if ([key isEqualToString:@"error"])
        return NO;
    else
        return [super automaticallyNotifiesObserversForKey:key];
}


- (P1State)state
{
    P1State state;

    [self lock];
    state = _state;
    [self unlock];

    return state;
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
- (void)setTarget:(P1TargetState)target
{
    [self lock];
    p1_object_target(_object, target);
    [self unlock];
}
+ (NSSet *)keyPathsForValuesAffectingTarget
{
    return [NSSet setWithObjects:@"state", nil];
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
