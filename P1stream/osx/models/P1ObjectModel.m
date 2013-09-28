#import "P1ObjectModel.h"


@implementation P1ObjectModel

- (id)initWithObject:(P1Object *)object
{
    self = [super init];
    if (self) {
        _object = object;
        _object->user_data = (__bridge void *)self;
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


// We hold a copy of the state, based on notifications, in order to stay KVO compliant.
+ (BOOL)automaticallyNotifiesObserversForKey:(NSString *)key
{
    if ([key isEqualToString:@"state"])
        return NO;
    else
        return [super automaticallyNotifiesObserversForKey:key];
}

- (void)handleNotification:(P1Notification *)n
{
    if (n->type == P1_NTYPE_STATE_CHANGE) {
        [self willChangeValueForKey:@"state"];
        _state = n->state_change.state;
        [self didChangeValueForKey:@"state"];
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
    p1_object_set_target(_object, target);
    [self unlock];
}


- (void)clearHalt
{
    [self lock];
    p1_object_clear_halt(_object);
    [self unlock];
}

@end
