#import "P1PreviewView.h"

#import <OpenGL/gl3.h>


static const char *vertexShader =
    "#version 150\n"

    "uniform sampler2DRect u_Texture;\n"
    "in vec2 a_Position;\n"
    "in vec2 a_TexCoords;\n"
    "out vec2 v_TexCoords;\n"

    "void main(void) {\n"
        "gl_Position = vec4(a_Position.x, a_Position.y, 0.0, 1.0);\n"
        "v_TexCoords = a_TexCoords * textureSize(u_Texture);\n"
    "}\n";

static const char *fragmentShader =
    "#version 150\n"

    "uniform sampler2DRect u_Texture;\n"
    "in vec2 v_TexCoords;\n"
    "out vec4 o_FragColor;\n"

    "void main(void) {\n"
        "o_FragColor = texture(u_Texture, v_TexCoords);\n"
    "}\n";

static const GLsizei vboStride = 4 * sizeof(GLfloat);
static const GLsizei vboSize = 4 * vboStride;
static const void *vboTexCoordOffset = (void *)(2 * sizeof(GLfloat));
static const GLfloat vboData[] = {
    -1, -1, 0, 1,
    -1, +1, 0, 0,
    +1, -1, 1, 1,
    +1, +1, 1, 0
};


static GLuint buildShader(GLuint type, const char *source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint logSize = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logSize);
    if (logSize) {
        GLchar *log = malloc(logSize);
        if (log) {
            glGetShaderInfoLog(shader, logSize, NULL, log);
            NSLog(@"Shader compiler log:\n%s", log);
            free(log);
        }
    }

    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        NSLog(@"Failed to build shader: OpenGL error %d", err);
        return 0;
    }
    if (success != GL_TRUE) {
        NSLog(@"Failed to build shader");
        return 0;
    }

    return shader;
}

static GLuint buildProgram()
{
    GLuint program = glCreateProgram();

    glBindAttribLocation(program, 0, "a_Position");
    glBindAttribLocation(program, 1, "a_TexCoords");
    glBindFragDataLocation(program, 0, "o_FragColor");

    GLuint vs = buildShader(GL_VERTEX_SHADER, vertexShader);
    if (vs == 0)
        return 0;

    GLuint fs = buildShader(GL_FRAGMENT_SHADER, fragmentShader);
    if (fs == 0)
        return 0;

    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDetachShader(program, vs);
    glDetachShader(program, fs);

    glDeleteShader(vs);
    glDeleteShader(fs);

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

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        NSLog(@"Failed to link shaders: OpenGL error %d", err);
        return 0;
    }
    if (success != GL_TRUE) {
        NSLog(@"Failed to link shaders");
        return 0;
    }

    return program;
}


@implementation P1PreviewView

- (id)initWithFrame:(NSRect)frameRect
{
    NSOpenGLPixelFormatAttribute attrs[] = {
        NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core,
        0
    };
    NSOpenGLPixelFormat *pf = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];

    return [super initWithFrame:frameRect pixelFormat:pf];
}

- (void)dealloc
{
    self.context = NULL;
}


- (BOOL)isOpaque
{
    return TRUE;
}

- (BOOL)mouseDownCanMoveWindow
{
    return TRUE;
}

- (void)prepareOpenGL
{
    glClearColor(0, 0, 0, 1);

    GLuint program = buildProgram();
    glUseProgram(program);

    glGenTextures(1, &_tex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_RECTANGLE, _tex);
    GLuint texUniform = glGetUniformLocation(program, "u_Texture");
    glUniform1i(texUniform, 0);

    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vboSize, vboData, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, vboStride, 0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, vboStride, vboTexCoordOffset);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    GLenum glErr = glGetError();
    if (glErr != GL_NO_ERROR)
        NSLog(@"Failed to create GL objects: OpenGL error %d", glErr);
}

- (void)reshape
{
    NSOpenGLContext *ctx = self.openGLContext;
    CGLContextObj cglCtx = ctx.CGLContextObj;

    CGSize size = self.frame.size;

    CGLLockContext(cglCtx);
    [ctx makeCurrentContext];

    glViewport(0, 0, size.width, size.height);

    CGLUnlockContext(cglCtx);
}

- (void)drawRect:(NSRect)dirtyRect
{
    [self render];
}


- (P1Context *)context
{
    return _context;
}

- (void)setContext:(P1Context *)context
{
    if (context == _context)
        return;

    if (_context)
        setVideoPreviewCallback(_context->video, NULL, NULL);

    _context = context;
    self.surface = NULL;

    if (_context)
        setVideoPreviewCallback(_context->video, videoPreviewCallback, (__bridge void *)self);
}


static void setVideoPreviewCallback(P1Video *video, P1VideoPreviewCallback fn, void *user_data)
{
    P1Object *videoobj = (P1Object *)video;

    p1_object_lock(videoobj);

    video->preview_fn = fn;
    video->preview_user_data = user_data;
    video->preview_type = P1_PREVIEW_IOSURFACE;

    p1_object_unlock(videoobj);
}

static void videoPreviewCallback(void *ptr, void *user_data)
{
    @autoreleasepool {
        P1PreviewView *self = (__bridge P1PreviewView *)user_data;

        self.surface = ptr;
        [self render];
    }
}

- (void)setAspect:(float)aspect
{
    if (aspect == _aspect)
        return;

    _aspect = aspect;

    if (_constraint) {
        [self removeConstraint:_constraint];
        _constraint = nil;
    }

    if (aspect != 0) {
        _constraint = [NSLayoutConstraint constraintWithItem:self
                                                   attribute:NSLayoutAttributeWidth
                                                   relatedBy:NSLayoutRelationEqual
                                                      toItem:self
                                                   attribute:NSLayoutAttributeHeight
                                                  multiplier:aspect
                                                    constant:0];
        [self addConstraint:_constraint];
    }
}

- (void)setSurface:(IOSurfaceRef)surface
{
    if (_surface == surface)
        return;

    if (_surface) {
        IOSurfaceDecrementUseCount(_surface);
        CFRelease(_surface);
    }

    _surface = surface;

    float aspect = 0;
    if (surface) {
        CFRetain(surface);
        IOSurfaceIncrementUseCount(surface);

        _width = (GLsizei)IOSurfaceGetWidth(surface);
        _height = (GLsizei)IOSurfaceGetHeight(surface);
        aspect = (float)_width / (float)_height;
    }

    if (aspect != _aspect) {
        dispatch_async(dispatch_get_main_queue(), ^{
            self.aspect = aspect;
        });
    }
}

- (void)render
{
    NSOpenGLContext *ctx = self.openGLContext;
    CGLContextObj cglCtx = ctx.CGLContextObj;

    CGLLockContext(cglCtx);
    [ctx makeCurrentContext];

    glClear(GL_COLOR_BUFFER_BIT);

    if (_surface) {
        glBindTexture(GL_TEXTURE_RECTANGLE, _tex);
        CGLError cgl_err = CGLTexImageIOSurface2D(cglCtx, GL_TEXTURE_RECTANGLE, GL_RGBA8, _width, _height,
                                                  GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, _surface, 0);
        if (cgl_err != kCGLNoError)
            NSLog(@"Failed to bind IOSurface to GL texture: Core Graphics error %d", cgl_err);
        else
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    glFinish();
    CGLUnlockContext(cglCtx);
}

@end
