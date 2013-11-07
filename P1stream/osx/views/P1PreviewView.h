@interface P1PreviewView : NSOpenGLView
{
    P1Context *_context;

    float _aspect;
    NSLayoutConstraint *_constraint;

    GLuint _tex;
    GLsizei _width;
    GLsizei _height;
    IOSurfaceRef _surface;
}

@property (assign) P1Context *context;

@end
