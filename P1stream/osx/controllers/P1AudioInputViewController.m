#import "P1AudioInputViewController.h"

#include <CoreAudio/CoreAudio.h>


@implementation P1AudioInputViewController

- (id)init
{
    return [super initWithNibName:@"AudioInputView" bundle:nil];
}

- (void)loadView
{
    [self refreshDevices:nil];
    [super loadView];
}

- (IBAction)refreshDevices:(id)sender
{
    UInt32 size;
    AudioObjectPropertyAddress addr = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };

    // Get a list of devices.
    OSStatus ret = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &addr, 0, NULL, &size);
    if (ret != kAudioHardwareNoError) {
        NSLog(@"Failed to get number of audio devices: Core Audio error %d", ret);
        return;
    }

    char dataForDeviceIds[size];
    AudioDeviceID *deviceIds = (AudioDeviceID *)dataForDeviceIds;
    ret = AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, NULL, &size, deviceIds);
    if (ret != kAudioHardwareNoError) {
        NSLog(@"Failed to get audio devices: Core Audio error %d", ret);
        return;
    }

    UInt32 numDevices = size / sizeof(AudioDeviceID);
    NSMutableArray *devices = [NSMutableArray arrayWithCapacity:numDevices];

    addr.mScope = kAudioDevicePropertyScopeInput;
    for (UInt32 i = 0; i < numDevices; i++) {
        // Check if the device has input channels.
        addr.mSelector = kAudioDevicePropertyStreamConfiguration;
        ret = AudioObjectGetPropertyDataSize(deviceIds[i], &addr, 0, NULL, &size);
        if (ret != kAudioHardwareNoError) {
            NSLog(@"Failed to get number of audio device channels: Core Audio error %d", ret);
            return;
        }

        char dataForBufferList[size];
        AudioBufferList *bufferList = (AudioBufferList *)dataForBufferList;
        ret = AudioObjectGetPropertyData(deviceIds[i], &addr, 0, NULL, &size, bufferList);
        if (ret != kAudioHardwareNoError) {
            NSLog(@"Failed to get number of audio device configuration: Core Audio error %d", ret);
            return;
        }

        UInt32 numChannels = 0;
        for (UInt32 j = 0; j < bufferList->mNumberBuffers; j++)
            numChannels += bufferList->mBuffers[j].mNumberChannels;
        if (numChannels == 0)
            continue;

        // Build a dictionary with devices properties.
        CFStringRef deviceUID;
        size = sizeof(deviceUID);
        addr.mSelector = kAudioDevicePropertyDeviceUID;
        ret = AudioObjectGetPropertyData(deviceIds[i], &addr, 0, NULL, &size, &deviceUID);
        if (ret != kAudioHardwareNoError) {
            NSLog(@"Failed to get audio device UID: Core Audio error %d", ret);
            return;
        }

        CFStringRef deviceName;
        size = sizeof(deviceName);
        addr.mSelector = kAudioDevicePropertyDeviceNameCFString;
        ret = AudioObjectGetPropertyData(deviceIds[i], &addr, 0, NULL, &size, &deviceName);
        if (ret != kAudioHardwareNoError) {
            NSLog(@"Failed to get audio device name: Core Audio error %d", ret);
            return;
        }

        [devices addObject:@{
            @"uid": CFBridgingRelease(deviceUID),
            @"name": CFBridgingRelease(deviceName)
        }];
    }

    self.devices = devices;
}

@end
