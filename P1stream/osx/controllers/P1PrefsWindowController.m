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
    [self revertPrefs:nil];

    _toolbar.selectedItemIdentifier = @"P1GeneralPage";
}

- (void)windowWillClose:(NSNotification *)notification
{
    if (_isDirty)
        [self revertPrefs:nil];
}

- (void)prefsDidChange:(P1PrefsDictionary *)prefs
{
    self.isDirty = TRUE;
}

- (IBAction)dummy:(id)sender
{
    // This is apparently needed to make toolbar items clickable.
}

- (IBAction)revertPrefs:(id)sender
{
    NSUserDefaults *ud = [NSUserDefaults standardUserDefaults];
    NSDictionary *dict;
    NSArray *arr;
    NSMutableArray *mutArr;

    dict = [ud dictionaryForKey:@"general"];
    self.generalConfig = [P1PrefsDictionary prefsWithDictionary:dict delegate:self];

    arr = [ud arrayForKey:@"audio-sources"];
    mutArr = [NSMutableArray arrayWithCapacity:arr.count];
    for (dict in arr)
        [mutArr addObject:[P1PrefsDictionary prefsWithDictionary:dict delegate:self]];
    self.audioSourceConfigs = mutArr;

    dict = [ud dictionaryForKey:@"video-clock"];
    self.videoClockConfig = [P1PrefsDictionary prefsWithDictionary:dict delegate:self];

    arr = [ud arrayForKey:@"video-sources"];
    mutArr = [NSMutableArray arrayWithCapacity:arr.count];
    for (dict in arr)
        [mutArr addObject:[P1PrefsDictionary prefsWithDictionary:dict delegate:self]];
    self.videoSourceConfigs = mutArr;

    self.isDirty = FALSE;
}

- (IBAction)applyPrefs:(id)sender
{
    NSUserDefaults *ud = [NSUserDefaults standardUserDefaults];
    P1PrefsDictionary *prefs;
    NSMutableArray *mutArr;

    [ud setObject:[_generalConfig.dictionary copy] forKey:@"general"];

    mutArr = [NSMutableArray arrayWithCapacity:_audioSourceConfigs.count];
    for (prefs in _audioSourceConfigs)
        [mutArr addObject:[prefs.dictionary copy]];
    [ud setObject:[mutArr copy] forKey:@"audio-sources"];

    [ud setObject:[_videoClockConfig.dictionary copy] forKey:@"video-clock"];

    mutArr = [NSMutableArray arrayWithCapacity:_videoSourceConfigs.count];
    for (prefs in _videoSourceConfigs)
        [mutArr addObject:[prefs.dictionary copy]];
    [ud setObject:[mutArr copy] forKey:@"video-sources"];

    [_contextModel reconfigure];

    self.isDirty = FALSE;
}

- (IBAction)selectedAudioSourceDidChange:(NSTableView *)sender
{
    NSDictionary *audioSourceConfig = nil;
    NSIndexSet *indexSet = sender.selectedRowIndexes;
    if (indexSet.count == 1)
        audioSourceConfig = _audioSourceConfigs[[indexSet firstIndex]];

    NSString *type = audioSourceConfig[@"type"];
    NSView *container = _audioSourceViewBox.contentView;

    Class viewClass = nil;
    if ([type isEqualToString:@"input"])
        viewClass = [P1AudioInputViewController class];

    if (viewClass) {
        self.audioSourceViewController = [viewClass new];
        _audioSourceViewController.representedObject = audioSourceConfig;

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
