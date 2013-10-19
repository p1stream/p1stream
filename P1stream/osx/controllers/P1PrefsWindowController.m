#import "P1PrefsWindowController.h"

#import "P1AudioInputViewController.h"

#include <x264.h>


@implementation P1PrefsWindowController

- (id)init
{
    self = [super initWithWindowNibName:@"PrefsWindow"];
    if (self) {
        _profileNames = arrayWithX264Names(x264_profile_names);
        _presetNames = arrayWithX264Names(x264_preset_names);
        _tuneNames = arrayWithX264Names(x264_tune_names);
    }
    return self;
}

static NSArray *arrayWithX264Names(char const * const *names)
{
    size_t i;

    // Count entries.
    i = 0;
    while (names[i]) i++;

    // Create NSStrings.
    NSString *nsNames[i];
    i = 0;
    while (names[i]) {
        nsNames[i] = [NSString stringWithCString:names[i] encoding:NSUTF8StringEncoding];
        i++;
    }

    return [NSArray arrayWithObjects:nsNames count:i];
}

- (void)windowDidLoad
{
    _toolbar.selectedItemIdentifier = @"P1GeneralPage";

    [_audioSourcesController addObserver:self
                              forKeyPath:@"selectedObjects"
                                 options:0
                                 context:NULL];
}

- (void)windowWillClose:(NSNotification *)notification
{
    if (_userDefaultsController.hasUnappliedChanges)
        [_userDefaultsController revert:nil];
}

- (IBAction)dummy:(id)sender
{
    // This is apparently needed to make toolbar items clickable.
}

- (IBAction)applySettings:(id)sender
{
    [_userDefaultsController save:sender];
    [_contextModel reconfigure];
}

- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary *)change
                       context:(void *)context
{
    if (object == _audioSourcesController) {
        NSDictionary *sourceDict = nil;
        if (_audioSourcesController.selectedObjects.count == 1)
            sourceDict =_audioSourcesController.selectedObjects[0];
        [self loadPanelForAudioSource:sourceDict];
    }
}

- (void)loadPanelForAudioSource:(NSDictionary *)sourceDict
{
    NSString *type = sourceDict[@"type"];
    NSView *container = _audioSourceViewBox.contentView;

    Class viewClass = nil;
    if ([type isEqualToString:@"input"])
        viewClass = [P1AudioInputViewController class];

    if (viewClass) {
        self.audioSourceViewController = [viewClass new];
        _audioSourceViewController.representedObject = sourceDict;

        [_audioSourceViewController loadView];
        NSView *view = _audioSourceViewController.view;

        container.subviews = @[view];
        view.frame = container.bounds;
    }
    else {
        self.audioSourceViewController = nil;
        container.subviews = @[];
    }
}

@end
