#import "P1Graphics.h"


NSArray *P1GraphicsGetDisplays()
{
    CGError ret;

    uint32_t numDisplays;
    ret = CGGetOnlineDisplayList(0, NULL, &numDisplays);
    if (ret != kCGErrorSuccess) {
        NSLog(@"Failed to get number of displays: Core Graphics error %d", ret);
        return nil;
    }

    CGDirectDisplayID displayIds[numDisplays];
    ret = CGGetOnlineDisplayList(numDisplays, displayIds, &numDisplays);
    if (ret != kCGErrorSuccess) {
        NSLog(@"Failed to enumerate displays: Core Graphics error %d", ret);
        return nil;
    }

    // FIXME: Mavericks deprecated the method we need to get a descriptive name.

    NSMutableArray *displays = [NSMutableArray arrayWithCapacity:numDisplays];
    for (uint32_t i = 0; i < numDisplays; i++) {
        [displays addObject:@{
            @"uid": [NSNumber numberWithUnsignedInt:displayIds[i]],
            @"name": [NSString stringWithFormat:@"0x%08x", displayIds[i]]
        }];
    }

    return displays;
}
