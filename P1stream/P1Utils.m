#import "P1Utils.h"


@implementation NSArray (Map)

- (NSMutableArray *)mapObjectsWithBlock:(id (^)(id obj, NSUInteger idx))block
{
    NSMutableArray *result = [NSMutableArray arrayWithCapacity:[self count]];
    [self enumerateObjectsUsingBlock:^(id obj, NSUInteger idx, BOOL *stop) {
        [result addObject:block(obj, idx)];
    }];
    return result;
}

@end


BOOL checkAndLogGLError(NSString *action)
{
    GLenum glError = glGetError();
    if (glError != GL_NO_ERROR) {
        NSLog(@"OpenGL error during %@: %s", action, gluErrorString(glError));
        return TRUE;
    }
    return FALSE;
}