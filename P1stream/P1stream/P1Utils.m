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


@implementation NSError (P1ErrorWithGError)

+ (NSError *)errorWithGError:(GError *)error;
{
    return [[NSError alloc] initWithGError:error];
}

- (NSError *)initWithGError:(GError *)error;
{
    const gchar *cDomain = g_quark_to_string(error->domain);
    NSString *domain = [NSString stringWithUTF8String:cDomain];
    NSString *message = [NSString stringWithUTF8String:error->message];
    return [self initWithDomain:domain code:error->code userInfo:@{
        @"message": message
    }];
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
