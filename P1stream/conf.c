#include <stdio.h>
#include <mach/mach_time.h>
#include <CoreFoundation/CoreFoundation.h>

#include "conf.h"

static void p1_conf_defaults();
static void p1_conf_finish();
static void p1_conf_read_plist(const char *path);
static CFTypeRef p1_conf_read_plist_value(CFDictionaryRef dict, const char *key, CFTypeID type);
static void p1_conf_read_plist_string(CFDictionaryRef dict, const char *key, char **dest);
static void p1_conf_read_plist_encoder(CFDictionaryRef dict);

struct p1_conf_t p1_conf;


void p1_conf_init(const char *file)
{
    p1_conf_defaults();
    p1_conf_read_plist(file);
    p1_conf_finish();
}

static void p1_conf_defaults()
{
    p1_conf.stream.url = strdup("rtmp://localhost/app/test");

    x264_param_default(&p1_conf.encoder);
}

static void p1_conf_finish()
{
    mach_timebase_info_data_t timebase;
    mach_timebase_info(&timebase);

    p1_conf.encoder.i_timebase_num = timebase.numer;
    p1_conf.encoder.i_timebase_den = timebase.denom * 1000000000;
    p1_conf.encoder.b_aud = 1;
    p1_conf.encoder.b_annexb = 0;
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
    if (stream)
        p1_conf_read_plist_string(stream, "url", &p1_conf.stream.url);

    CFDictionaryRef encoder = p1_conf_read_plist_value(root, "encoder", CFDictionaryGetTypeID());
    if (encoder)
        p1_conf_read_plist_encoder(encoder);

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

static void p1_conf_read_plist_encoder(CFDictionaryRef dict)
{
    Boolean b_res;
    int i_res;
    char *tmp = NULL;

    p1_conf_read_plist_string(dict, "preset", &tmp);
    if (tmp) {
        i_res = x264_param_default_preset(&p1_conf.encoder, tmp, NULL);
        assert(i_res == 0);
    }

    CFIndex count = CFDictionaryGetCount(dict);
    const void *keys[count];
    const void *values[count];
    CFDictionaryGetKeysAndValues(dict, keys, values);
    for (CFIndex i = 0; i < count; i++) {
        CFStringRef str_key = keys[i];
        assert(CFGetTypeID(str_key) == CFStringGetTypeID());

        CFStringRef str_val = values[i];
        assert(CFGetTypeID(str_val) == CFStringGetTypeID());

        if (CFStringCompare(str_key, CFSTR("preset"), 0) == kCFCompareEqualTo)
            continue;
        if (CFStringCompare(str_key, CFSTR("profile"), 0) == kCFCompareEqualTo)
            continue;

        CFIndex size;

        size = CFStringGetLength(str_key) + 1;
        char key[size];
        b_res = CFStringGetCString(str_key, key, size, kCFStringEncodingASCII);
        assert(b_res == TRUE);

        size = CFStringGetLength(str_val) + 1;
        char val[size];
        b_res = CFStringGetCString(str_val, val, size, kCFStringEncodingASCII);
        assert(b_res == TRUE);

        i_res = x264_param_parse(&p1_conf.encoder, key, val);
        assert(i_res == 0);
    }

    x264_param_apply_fastfirstpass(&p1_conf.encoder);

    p1_conf_read_plist_string(dict, "profile", &tmp);
    if (tmp) {
        i_res = x264_param_apply_profile(&p1_conf.encoder, tmp);
        assert(i_res == 0);

        free(tmp);
    }
}
