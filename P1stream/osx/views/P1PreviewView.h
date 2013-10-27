@interface P1PreviewView : NSView
{
    P1Context *_context;

    float _aspect;
    NSLayoutConstraint *_constraint;

    const uint8_t *_lastData;
    CGDataProviderRef _dataProvider;
    CGColorSpaceRef _colorSpace;
}

@property (assign) P1Context *context;

@end
