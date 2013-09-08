#include "p1stream.h"

#include <string.h>


typedef struct _P1PlistConfig P1PlistConfig;

struct _P1PlistConfig {
    P1Config super;

    CFDictionaryRef root;
};

static void p1_plist_config_free(P1Config *_cfg);
static CFTypeRef p1_plist_config_resolve(P1Config *_cfg, P1ConfigSection *sect, const char *key, CFTypeID type);
static P1ConfigSection *p1_plist_config_get_section(P1Config *_cfg, P1ConfigSection *sect, const char *key);
static bool p1_plist_config_get_string(P1Config *_cfg, P1ConfigSection *sect, const char *key, char *buf, size_t bufsize);
static bool p1_plist_config_get_float(P1Config *_cfg, P1ConfigSection *sect, const char *key, float *out);
static bool p1_plist_config_get_bool(P1Config *_cfg, P1ConfigSection *sect, const char *key, bool *out);
static bool p1_plist_config_each_section(P1Config *_cfg, P1ConfigSection *sect, const char *key, P1ConfigIterSection iter, void *data);
static bool p1_plist_config_each_string(P1Config *_cfg, P1ConfigSection *sect, const char *key, P1ConfigIterString iter, void *data);


P1Config *p1_plist_config_create(CFDictionaryRef root)
{
    P1PlistConfig *cfg = calloc(1, sizeof(P1PlistConfig));
    assert(cfg != NULL);

    cfg->root = CFRetain(root);

    P1Config *_cfg = (P1Config *) cfg;
    _cfg->free = p1_plist_config_free;
    _cfg->get_section = p1_plist_config_get_section;
    _cfg->get_string = p1_plist_config_get_string;
    _cfg->get_float = p1_plist_config_get_float;
    _cfg->get_bool = p1_plist_config_get_bool;
    _cfg->each_section = p1_plist_config_each_section;
    _cfg->each_string = p1_plist_config_each_string;
    return _cfg;
}

P1Config *p1_plist_config_create_from_file(const char *file)
{
    CFURLRef file_url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)file, strlen(file), FALSE);
    assert(file_url != NULL);

    CFDataRef file_data;
    Boolean res = CFURLCreateDataAndPropertiesFromResource(NULL, file_url, &file_data, NULL, NULL, NULL);
    CFRelease(file_url);
    assert(res == TRUE);

    CFDictionaryRef root = CFPropertyListCreateWithData(NULL, file_data, kCFPropertyListImmutable, NULL, NULL);
    CFRelease(file_data);
    assert(root != NULL);
    assert(CFGetTypeID(root) == CFDictionaryGetTypeID());

    P1Config *_cfg = p1_plist_config_create(root);
    CFRelease(root);
    return _cfg;
}

static void p1_plist_config_free(P1Config *_cfg)
{
    P1PlistConfig *cfg = (P1PlistConfig *) _cfg;

    CFRelease(cfg->root);
    free(cfg);
}

static CFTypeRef p1_plist_config_resolve(P1Config *_cfg, P1ConfigSection *sect, const char *key, CFTypeID type)
{
    P1PlistConfig *cfg = (P1PlistConfig *) _cfg;

    CFTypeRef val = (CFDictionaryRef) sect;
    if (val == NULL)
        val = cfg->root;

    char *key_copy = strdup(key);
    char *key_iter = key_copy;
    char *token;
    while (val != NULL) {
        token = strsep(&key_iter, ".");
        if (token == NULL)
            break;

        if (CFGetTypeID(val) == CFDictionaryGetTypeID()) {
            CFStringRef token_str = CFStringCreateWithCStringNoCopy(NULL, token, kCFStringEncodingUTF8, kCFAllocatorNull);
            if (token_str != NULL) {
                val = CFDictionaryGetValue(val, token_str);
                CFRelease(token_str);
                continue;
            }
        }
        val = NULL;
    }
    free(key_copy);

    if (val && CFGetTypeID(val) != type)
        val = NULL;

    return val;
}

static P1ConfigSection *p1_plist_config_get_section(P1Config *_cfg, P1ConfigSection *sect, const char *key)
{
    return (P1ConfigSection *) p1_plist_config_resolve(_cfg, sect, key, CFDictionaryGetTypeID());
}

static bool p1_plist_config_get_string(P1Config *_cfg, P1ConfigSection *sect, const char *key, char *buf, size_t bufsize)
{
    CFStringRef val = p1_plist_config_resolve(_cfg, sect, key, CFStringGetTypeID());
    if (val) {
        Boolean res = CFStringGetCString(val, buf, bufsize, kCFStringEncodingUTF8);
        if (res == TRUE)
            return true;
    }
    return false;
}

static bool p1_plist_config_get_float(P1Config *_cfg, P1ConfigSection *sect, const char *key, float *out)
{
    CFNumberRef val = p1_plist_config_resolve(_cfg, sect, key, CFNumberGetTypeID());
    if (val) {
        Boolean res = CFNumberGetValue(val, kCFNumberFloatType, out);
        if (res == TRUE)
            return true;
    }
    return false;
}

static bool p1_plist_config_get_bool(P1Config *_cfg, P1ConfigSection *sect, const char *key, bool *out)
{
    CFBooleanRef val = p1_plist_config_resolve(_cfg, sect, key, CFBooleanGetTypeID());
    if (val) {
        *out = CFBooleanGetValue(val) == TRUE ? true : false;
        return true;
    }
    return false;
}

static bool p1_plist_config_each_section(P1Config *_cfg, P1ConfigSection *sect, const char *key, P1ConfigIterSection iter, void *data)
{
    CFArrayRef arr = p1_plist_config_resolve(_cfg, sect, key, CFArrayGetTypeID());
    if (arr == NULL)
        return true;

    CFIndex count = CFArrayGetCount(arr);
    const void *values[count];
    CFArrayGetValues(arr, CFRangeMake(0, count), values);
    for (CFIndex i = 0; i < count; i++) {
        CFDictionaryRef dict = values[i];
        assert(CFGetTypeID(dict) == CFDictionaryGetTypeID());

        if (iter(_cfg, (P1ConfigSection *) dict, data) == false)
            return false;
    }

    return true;
}

static bool p1_plist_config_each_string(P1Config *_cfg, P1ConfigSection *sect, const char *key, P1ConfigIterString iter, void *data)
{
    CFDictionaryRef dict = p1_plist_config_resolve(_cfg, sect, key, CFDictionaryGetTypeID());
    if (dict == NULL)
        return true;

    CFIndex count = CFDictionaryGetCount(dict);
    const void *keys[count];
    const void *values[count];
    CFDictionaryGetKeysAndValues(dict, keys, values);
    for (CFIndex i = 0; i < count; i++) {
        CFStringRef str_key = keys[i];
        assert(CFGetTypeID(str_key) == CFStringGetTypeID());

        CFStringRef str_val = values[i];
        assert(CFGetTypeID(str_val) == CFStringGetTypeID());

        Boolean b_res;
        CFIndex size;

        size = CFStringGetLength(str_key) + 1;
        char key[size];
        b_res = CFStringGetCString(str_key, key, size, kCFStringEncodingUTF8);
        if (b_res == FALSE)
            continue;

        size = CFStringGetLength(str_val) + 1;
        char val[size];
        b_res = CFStringGetCString(str_val, val, size, kCFStringEncodingUTF8);
        if (b_res == FALSE)
            continue;

        if (iter(_cfg, key, val, data) == false)
            return false;
    }

    return true;
}
