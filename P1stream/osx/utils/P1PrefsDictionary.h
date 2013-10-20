@protocol P1PrefsDictionaryDelegate;


// This class simply wraps the basic NSMutableDictionary with a delegate that
// receives a message for every change.
@interface P1PrefsDictionary : NSObject <NSCopying>
{
    NSMutableDictionary *_dictionary;
}

@property (weak) id<P1PrefsDictionaryDelegate> delegate;

// Creates a mutable copy of the dictionary.
+ (id)prefsWithDictionary:(NSDictionary *)dictionary
                 delegate:(id<P1PrefsDictionaryDelegate>)delegate;
- (id)initWithDictionary:(NSDictionary *)dictionary
                 delegate:(id<P1PrefsDictionaryDelegate>)delegate;

- (id)objectForKey:(id)aKey;
- (id)objectForKeyedSubscript:(id)key;
- (void)removeObjectForKey:(id)aKey;
- (void)setObject:(id)anObject forKey:(id <NSCopying>)aKey;

@end


@protocol P1PrefsDictionaryDelegate

- (void)prefsDidChange:(P1PrefsDictionary *)prefs;

@end