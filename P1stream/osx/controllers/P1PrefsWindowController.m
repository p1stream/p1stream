#import "P1PrefsWindowController.h"


@implementation P1PrefsWindowController

- (id)init
{
    return [super initWithWindowNibName:@"PrefsWindow"];
}

- (void)windowDidLoad
{
    [self updateSelectedToolbarItem];
    [_tabView addObserver:self forKeyPath:@"selectedTabViewItem" options:0 context:nil];
}

- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary *)change
                       context:(void *)context
{
    if (object == _tabView && [keyPath isEqualToString:@"selectedTabViewItem"])
        [self updateSelectedToolbarItem];
}

- (void)updateSelectedToolbarItem
{
    _toolbar.selectedItemIdentifier = _tabView.selectedTabViewItem.identifier;
}

- (IBAction)selectTabForToolbarItem:(NSToolbarItem *)item
{
    [_tabView selectTabViewItemWithIdentifier:item.itemIdentifier];
}

@end
