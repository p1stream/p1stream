@interface NSArray (P1ArrayWithMap)

- (NSMutableArray *)mapObjectsWithBlock:(id (^)(id obj, NSUInteger idx))block;

@end


BOOL checkAndLogGLError(NSString *action);