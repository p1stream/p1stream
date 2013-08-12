#include <stdio.h>
#include <errno.h>
#include <CoreFoundation/CoreFoundation.h>

#include "conf.h"

static void p1_conf_defaults();
static void p1_conf_read_plist(const char *path);
static CFTypeRef p1_conf_read_plist_value(CFDictionaryRef dict, const char *key, CFTypeID type);
static void p1_conf_read_plist_string(CFDictionaryRef dict, const char *key, char **dest);

struct p1_conf_t p1_conf;


void p1_conf_init(const char *file)
{
    p1_conf_defaults();
    p1_conf_read_plist(file);
}

static void p1_conf_defaults()
{
    p1_conf.stream.url = strdup("rtmp://localhost/app/test");

    p1_conf.encoder.preset = strdup("veryfast");
    p1_conf.encoder.profile = strdup("baseline");
}

static void p1_conf_read_plist(const char *file)
{
    Boolean res;

    CFURLRef file_url = CFURLCreateFromFileSystemRepresentation(NULL, (UInt8 *)file, strlen(file), FALSE);
    assert(file_url != NULL);
    CFURLCreateData(NULL, file_url, kCFStringEncodingUTF8, FALSE);

    CFDataRef file_data;
    res = CFURLCreateDataAndPropertiesFromResource(NULL, file_url, &file_data, NULL, NULL, NULL);
    CFRelease(file_url);
    assert(res == TRUE);

    CFDictionaryRef root = CFPropertyListCreateWithData(NULL, file_data, kCFPropertyListImmutable, NULL, NULL);
    CFRelease(file_data);
    assert(root != NULL);
    assert(CFGetTypeID(root) == CFDictionaryGetTypeID());

    CFDictionaryRef stream = p1_conf_read_plist_value(root, "stream", CFDictionaryGetTypeID());
    if (stream) {
        p1_conf_read_plist_string(stream, "url", &p1_conf.stream.url);
    }

    CFDictionaryRef encoder = p1_conf_read_plist_value(root, "encoder", CFDictionaryGetTypeID());
    if (encoder) {
        p1_conf_read_plist_string(encoder, "preset", &p1_conf.encoder.preset);
        p1_conf_read_plist_string(encoder, "profile", &p1_conf.encoder.profile);
    }

    CFRelease(root);
}

static CFTypeRef p1_conf_read_plist_value(CFDictionaryRef dict, const char *key, CFTypeID type)
{
    CFStringRef key_str = CFStringCreateWithCStringNoCopy(NULL, key, kCFStringEncodingASCII, kCFAllocatorNull);
    assert(key_str != NULL);

    CFTypeRef val = CFDictionaryGetValue(dict, key_str);
    CFRelease(key_str);

    if (val && CFGetTypeID(val) != type)
        val = NULL;

    return val;
}

static void p1_conf_read_plist_string(CFDictionaryRef dict, const char *key, char **dest)
{
    CFStringRef val = p1_conf_read_plist_value(dict, key, CFStringGetTypeID());
    if (val) {
        CFIndex size = CFStringGetLength(val) + 1;

        if (*dest)
            free(*dest);
        *dest = malloc(size);

        Boolean res = CFStringGetCString(val, *dest, size, kCFStringEncodingASCII);
        assert(res == TRUE);
    }
}
