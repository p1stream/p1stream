#import "P1PrefsWindowController.h"

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

@end
