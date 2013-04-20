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


GLuint buildShader(GLuint type, NSString *resource, NSString *ext)
{
    NSString *path = [[NSBundle mainBundle] pathForResource:resource ofType:ext];
    NSData *data = [NSData dataWithContentsOfFile:path];
    if (!data) return 0;

    GLuint shader = glCreateShader(type);
    const char *source = [data bytes];
    const GLint length = (GLint)[data length];
    glShaderSource(shader, 1, &source, &length);
    glCompileShader(shader);

    GLint logSize = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logSize);
    if (logSize) {
        GLchar *log = malloc(logSize);
        if (log) {
            glGetShaderInfoLog(shader, logSize, NULL, log);
            NSLog(@"Shader compiler log for '%@':\n%s", resource, log);
            free(log);
        }
    }

    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    g_assert(success == GL_TRUE);
    g_assert(glGetError() == GL_NO_ERROR);

    return shader;
}

void buildShaderProgram(GLuint program, NSString *resource)
{
    GLuint vertexShader = buildShader(GL_VERTEX_SHADER, resource, @"vert.sl");
    GLuint fragmentShader = buildShader(GL_FRAGMENT_SHADER, resource, @"frag.sl");

    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    glDetachShader(program, vertexShader);
    glDetachShader(program, fragmentShader);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint logSize = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logSize);
    if (logSize) {
        GLchar *log = malloc(logSize);
        if (log) {
            glGetProgramInfoLog(program, logSize, NULL, log);
            NSLog(@"Shader linker log:\n%s", log);
            free(log);
        }
    }

    GLint success = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    g_assert(success == GL_TRUE);
    g_assert(glGetError() == GL_NO_ERROR);
}
