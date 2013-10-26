#import "P1PrefsWindowController.h"

#import "P1InputAudioSourceViewController.h"
#import "P1DisplayVideoClockViewController.h"
#import "P1DisplayVideoSourceViewController.h"
#import "P1CaptureVideoSourceViewController.h"

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

    [self videoClockDidChange];
}

- (IBAction)applyPrefs:(id)sender
{
    NSUserDefaults *ud = [NSUserDefaults standardUserDefaults];
    P1PrefsDictionary *prefs;
    NSMutableArray *mutArr;

    [ud setObject:[_generalConfig copy] forKey:@"general"];

    mutArr = [NSMutableArray arrayWithCapacity:_audioSourceConfigs.count];
    for (prefs in _audioSourceConfigs)
        [mutArr addObject:[prefs copy]];
    [ud setObject:mutArr forKey:@"audio-sources"];

    [ud setObject:[_videoClockConfig copy] forKey:@"video-clock"];

    mutArr = [NSMutableArray arrayWithCapacity:_videoSourceConfigs.count];
    for (prefs in _videoSourceConfigs)
        [mutArr addObject:[prefs copy]];
    [ud setObject:mutArr forKey:@"video-sources"];

    [_contextModel reconfigure];

    self.isDirty = FALSE;
}


- (IBAction)selectedAudioSourceDidChange:(id)sender
{
    P1PrefsDictionary *audioSourceConfig = nil;
    NSIndexSet *indexSet = _audioSourcesTableView.selectedRowIndexes;
    if (indexSet.count == 1)
        audioSourceConfig = _audioSourceConfigs[[indexSet firstIndex]];

    NSString *type = audioSourceConfig[@"type"];
    NSView *container = _audioSourceViewBox.contentView;

    Class viewClass = nil;
    if ([type isEqualToString:@"input"])
        viewClass = [P1InputAudioSourceViewController class];

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

- (IBAction)removeSelectedAudioSource:(id)sender
{
    NSIndexSet *indexSet = _audioSourcesTableView.selectedRowIndexes;
    if (indexSet.count == 0)
        return;

    [self willChange:NSKeyValueChangeRemoval valuesAtIndexes:indexSet forKey:@"audioSourceConfigs"];
    [_audioSourceConfigs removeObjectsAtIndexes:indexSet];
    [self didChange:NSKeyValueChangeRemoval valuesAtIndexes:indexSet forKey:@"audioSourceConfigs"];

    self.isDirty = TRUE;

    [self selectedAudioSourceDidChange:sender];
}

- (void)addAudioSource:(NSDictionary *)dict sender:(id)sender
{
    P1PrefsDictionary *audioSourceConfig = [P1PrefsDictionary prefsWithDictionary:dict delegate:nil];

    CFUUIDRef cfUuid = CFUUIDCreate(NULL);
    audioSourceConfig[@"uuid"] = CFBridgingRelease(CFUUIDCreateString(NULL, cfUuid));
    CFRelease(cfUuid);

    audioSourceConfig.delegate = self;

    NSUInteger count = _audioSourceConfigs.count;
    NSIndexSet *indexSet = [NSIndexSet indexSetWithIndex:count];

    [self willChange:NSKeyValueChangeInsertion valuesAtIndexes:indexSet forKey:@"audioSourceConfigs"];
    [_audioSourceConfigs insertObject:audioSourceConfig atIndex:count];
    [self didChange:NSKeyValueChangeInsertion valuesAtIndexes:indexSet forKey:@"audioSourceConfigs"];

    self.isDirty = TRUE;

    [_audioSourcesTableView selectRowIndexes:indexSet byExtendingSelection:FALSE];
    [self selectedAudioSourceDidChange:sender];
}

- (IBAction)addInputAudioSource:(id)sender
{
    [self addAudioSource:@{
        @"name": @"Input device source",
        @"type": @"input"
    } sender:sender];
}


- (void)videoClockDidChange
{
    NSString *type = _videoClockConfig[@"type"];
    NSView *container = _videoClockViewBox.contentView;

    Class viewClass = nil;
    NSNumber *menuIndex = @-1;
    if ([type isEqualToString:@"display"]) {
        viewClass = [P1DisplayVideoClockViewController class];
        menuIndex = @0;
    }

    _videoClockPopUpButton.objectValue = menuIndex;

    if (viewClass) {
        self.videoClockViewController = [viewClass new];
        _videoClockViewController.representedObject = _videoClockConfig;

        [_videoClockViewController loadView];
        NSView *view = _videoClockViewController.view;

        container.subviews = @[view];
        view.frame = container.bounds;
    }
    else {
        self.videoClockViewController = nil;
        container.subviews = @[];
    }
}

- (void)setVideoClockType:(NSDictionary *)dict
{
    if ([_videoClockConfig[@"type"] isEqualToString:dict[@"type"]])
        return;

    P1PrefsDictionary *videoClockConfig = [P1PrefsDictionary prefsWithDictionary:dict delegate:nil];

    CFUUIDRef cfUuid = CFUUIDCreate(NULL);
    videoClockConfig[@"uuid"] = CFBridgingRelease(CFUUIDCreateString(NULL, cfUuid));
    CFRelease(cfUuid);

    videoClockConfig.delegate = self;

    self.videoClockConfig = videoClockConfig;

    self.isDirty = TRUE;

    [self videoClockDidChange];
}

- (IBAction)createDisplayVideoClock:(id)sender
{
    [self setVideoClockType:@{
        @"type": @"display"
    }];
}


- (IBAction)selectedVideoSourceDidChange:(id)sender
{
    P1PrefsDictionary *videoSourceConfig = nil;
    NSIndexSet *indexSet = _videoSourcesTableView.selectedRowIndexes;
    if (indexSet.count == 1)
        videoSourceConfig = _videoSourceConfigs[[indexSet firstIndex]];

    NSString *type = videoSourceConfig[@"type"];
    NSView *container = _videoSourceViewBox.contentView;

    Class viewClass = nil;
    if ([type isEqualToString:@"display"])
        viewClass = [P1DisplayVideoSourceViewController class];
    else if ([type isEqualToString:@"capture"])
        viewClass = [P1CaptureVideoSourceViewController class];

    if (viewClass) {
        self.videoSourceViewController = [viewClass new];
        _videoSourceViewController.representedObject = videoSourceConfig;

        [_videoSourceViewController loadView];
        NSView *view = _videoSourceViewController.view;

        container.subviews = @[view];
        view.frame = container.bounds;
    }
    else {
        self.videoSourceViewController = nil;
        container.subviews = @[];
    }
}

- (IBAction)removeSelectedVideoSource:(id)sender
{
    NSIndexSet *indexSet = _videoSourcesTableView.selectedRowIndexes;
    if (indexSet.count == 0)
        return;

    [self willChange:NSKeyValueChangeRemoval valuesAtIndexes:indexSet forKey:@"videoSourceConfigs"];
    [_videoSourceConfigs removeObjectsAtIndexes:indexSet];
    [self didChange:NSKeyValueChangeRemoval valuesAtIndexes:indexSet forKey:@"videoSourceConfigs"];

    self.isDirty = TRUE;

    [self selectedVideoSourceDidChange:sender];
}

- (void)addVideoSource:(NSDictionary *)dict sender:(id)sender
{
    P1PrefsDictionary *videoSourceConfig = [P1PrefsDictionary prefsWithDictionary:dict delegate:nil];

    CFUUIDRef cfUuid = CFUUIDCreate(NULL);
    videoSourceConfig[@"uuid"] = CFBridgingRelease(CFUUIDCreateString(NULL, cfUuid));
    CFRelease(cfUuid);

    videoSourceConfig.delegate = self;

    NSUInteger count = _videoSourceConfigs.count;
    NSIndexSet *indexSet = [NSIndexSet indexSetWithIndex:count];

    [self willChange:NSKeyValueChangeInsertion valuesAtIndexes:indexSet forKey:@"videoSourceConfigs"];
    [_videoSourceConfigs insertObject:videoSourceConfig atIndex:count];
    [self didChange:NSKeyValueChangeInsertion valuesAtIndexes:indexSet forKey:@"videoSourceConfigs"];

    self.isDirty = TRUE;

    [_videoSourcesTableView selectRowIndexes:indexSet byExtendingSelection:FALSE];
    [self selectedVideoSourceDidChange:sender];
}

- (IBAction)addDisplayVideoSource:(id)sender
{
    [self addVideoSource:@{
        @"name": @"Display source",
        @"type": @"display"
    } sender:sender];
}

- (IBAction)addCaptureVideoSource:(id)sender
{
    [self addVideoSource:@{
        @"name": @"Capture device source",
        @"type": @"capture"
    } sender:sender];
}

@end
