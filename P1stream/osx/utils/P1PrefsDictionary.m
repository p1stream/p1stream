#import "P1PrefsDictionary.h"


@implementation P1PrefsDictionary

+ (id)prefsWithDictionary:(NSDictionary *)dictionary
                 delegate:(id<P1PrefsDictionaryDelegate>)delegate
{
    return [[P1PrefsDictionary alloc] initWithDictionary:dictionary
                                                delegate:delegate];
}

- (id)initWithDictionary:(NSDictionary *)dictionary
                delegate:(id<P1PrefsDictionaryDelegate>)delegate
{
    self = [super init];
    if (self) {
        _dictionary = [dictionary mutableCopy];
        _delegate = delegate;
    }
    return self;
}

- (id)copyWithZone:(NSZone *)zone;
{
    return [_dictionary copyWithZone:zone];
}

// NSDictionary
- (id)objectForKey:(id)aKey
{
    return [_dictionary objectForKey:aKey];
}

// NSDictionary (NSExtendedDictionary)
- (id)objectForKeyedSubscript:(id)key
{
    return [_dictionary objectForKeyedSubscript:key];
}

// NSMutableDictionary
- (void)removeObjectForKey:(id)aKey
{
    [_dictionary removeObjectForKey:aKey];
    [_delegate prefsDidChange:self];
}

- (void)setObject:(id)anObject forKey:(id<NSCopying>)aKey
{
    [_dictionary setObject:anObject forKey:aKey];
    [_delegate prefsDidChange:self];
}

// NSMutableDictionary (NSExtendedMutableDictionary)
- (void)setObject:(id)object forKeyedSubscript:(id<NSCopying>)aKey
{
    [_dictionary setObject:object forKeyedSubscript:aKey];
    [_delegate prefsDidChange:self];
}

//  NSObject (NSKeyValueCoding)
- (id)valueForKey:(NSString *)key
{
    return [_dictionary valueForKey:key];
}

- (void)setValue:(id)value forKey:(NSString *)key
{
    [_dictionary setValue:value forKey:key];
    [_delegate prefsDidChange:self];
}

@end
