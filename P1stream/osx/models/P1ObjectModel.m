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


- (void)restart
{
    [self lock];

    if (_state == P1_STATE_IDLE) {
        _restart = false;
        self.target = P1_TARGET_RUNNING;
    }
    else {
        _restart = true;
        self.target = P1_TARGET_IDLE;
    }

    [self unlock];
}


// We hold a copy of the state, based on notifications, in order to stay KVO compliant.
+ (BOOL)automaticallyNotifiesObserversForKey:(NSString *)key
{
    if ([key isEqualToString:@"state"])
        return NO;
    else if ([key isEqualToString:@"error"])
        return NO;
    else
        return [super automaticallyNotifiesObserversForKey:key];
}
- (void)handleNotification:(P1Notification *)n
{
    if (n->type == P1_NTYPE_STATE_CHANGE) {
        P1State state = n->state_change.state;
        if (state != _state) {
            [self willChangeValueForKey:@"state"];
            _state = state;
            [self didChangeValueForKey:@"state"];
        }

        BOOL error = (n->state_change.flags & P1_FLAG_ERROR) ? TRUE : FALSE;
        if (error != _error) {
            [self willChangeValueForKey:@"error"];
            _error = error;
            [self didChangeValueForKey:@"error"];
        }

        if (_restart && state == P1_STATE_IDLE) {
            _restart = false;
            self.target = P1_TARGET_RUNNING;
        }
    }
}


// Target is backed directly by the real target field.
// This should be fine, because this property is the only way we access target.
- (P1TargetState)target
{
    return _object->target;
}
- (void)setTarget:(P1TargetState)target
{
    [self lock];

    _restart = false;

    [self willChangeValueForKey:@"error"];
    p1_object_set_target(_object, target);
    [self didChangeValueForKey:@"error"];

    [self unlock];
}


- (NSImage *)availabilityImage
{
    NSString *imageName;

    if (_error) {
        imageName = NSImageNameStatusUnavailable;
    }
    else {
        switch (_state) {
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
    return [NSSet setWithObjects:@"state", @"error", nil];
}

@end
