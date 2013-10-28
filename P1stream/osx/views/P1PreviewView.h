@interface P1PreviewView : NSOpenGLView
{
    P1Context *_context;

    float _aspect;
    NSLayoutConstraint *_constraint;

    GLuint _tex;
}

@property (assign) P1Context *context;

@end
