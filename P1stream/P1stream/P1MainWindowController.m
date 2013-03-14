#import "P1MainWindowController.h"


@implementation P1MainWindowController

- (void)showWindow:(id)sender
{
    [super showWindow:sender];

    NSView *contentView = self.window.contentView;

    preview = [[P1Preview alloc] initWithFrame:contentView.frame];
    preview.translatesAutoresizingMaskIntoConstraints = FALSE;
    [contentView addSubview:preview];

    NSDictionary *views = NSDictionaryOfVariableBindings(preview);
    NSArray *a, *b;
    a = [NSLayoutConstraint constraintsWithVisualFormat:@"H:|-0-[preview]-0-|"
                                                options:0
                                                metrics:nil
                                                  views:views];
    b = [NSLayoutConstraint constraintsWithVisualFormat:@"V:|-0-[preview]-0-|"
                                                options:0
                                                metrics:nil
                                                  views:views];
    constraints = [a arrayByAddingObjectsFromArray:b];
    [contentView addConstraints:constraints];

    pipeline = [[P1Pipeline alloc] initWithPreview:preview];
    [pipeline start];
}

- (void)windowWillClose:(NSNotification *)notification
{
    NSView *contentView = self.window.contentView;

    [pipeline stop];
    pipeline = nil;

    [contentView removeConstraints:constraints];
    constraints = nil;

    [preview removeFromSuperview];
    preview = nil;
}

@end
