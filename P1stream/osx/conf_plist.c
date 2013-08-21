#include <string.h>
#include <CoreFoundation/CoreFoundation.h>

#include "p1stream.h"


typedef struct _P1PlistConfig P1PlistConfig;

struct _P1PlistConfig {
    P1Config super;

    CFDictionaryRef root;
};

static void p1_plist_config_free(P1Config *_cfg);
static CFTypeRef p1_plist_config_resolve(P1Config *_cfg, P1ConfigSection *sect, const char *key, CFTypeID type);
static P1ConfigSection *p1_plist_config_get_section(P1Config *_cfg, P1ConfigSection *sect, const char *key);
static bool p1_plist_config_get_string(P1Config *_cfg, P1ConfigSection *sect, const char *key, char *buf, size_t bufsize);
static bool p1_plist_config_each_section(P1Config *_cfg, P1ConfigSection *sect, const char *key, P1ConfigIterSection iter, void *data);
static bool p1_plist_config_each_string(P1Config *_cfg, P1ConfigSection *sect, const char *key, P1ConfigIterString iter, void *data);


P1Config *p1_conf_plist_from_file(const char *file)
{
    P1PlistConfig *cfg = calloc(1, sizeof(P1PlistConfig));
    assert(cfg != NULL);

    Boolean res;

    CFURLRef file_url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)file, strlen(file), FALSE);
    assert(file_url != NULL);
    CFURLCreateData(NULL, file_url, kCFStringEncodingUTF8, FALSE);

    CFDataRef file_data;
    res = CFURLCreateDataAndPropertiesFromResource(NULL, file_url, &file_data, NULL, NULL, NULL);
    CFRelease(file_url);
    assert(res == TRUE);

    cfg->root = CFPropertyListCreateWithData(NULL, file_data, kCFPropertyListImmutable, NULL, NULL);
    CFRelease(file_data);
    assert(cfg->root != NULL);
    assert(CFGetTypeID(cfg->root) == CFDictionaryGetTypeID());

    P1Config *_cfg = (P1Config *) cfg;
    _cfg->free = p1_plist_config_free;
    _cfg->get_section = p1_plist_config_get_section;
    _cfg->get_string = p1_plist_config_get_string;
    _cfg->each_section = p1_plist_config_each_section;
    _cfg->each_string = p1_plist_config_each_string;
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
            CFStringRef token_str = CFStringCreateWithCStringNoCopy(NULL, token, kCFStringEncodingASCII, kCFAllocatorNull);
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
        Boolean res = CFStringGetCString(val, buf, bufsize, kCFStringEncodingASCII);
        if (res == TRUE)
            return true;
    }
    return false;
}

static bool p1_plist_config_each_section(P1Config *_cfg, P1ConfigSection *sect, const char *key, P1ConfigIterSection iter, void *data)
{
    // FIXME
    return false;
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
        b_res = CFStringGetCString(str_key, key, size, kCFStringEncodingASCII);
        if (b_res == FALSE)
            continue;

        size = CFStringGetLength(str_val) + 1;
        char val[size];
        b_res = CFStringGetCString(str_val, val, size, kCFStringEncodingASCII);
        if (b_res == FALSE)
            continue;

        if (iter(_cfg, key, val, data) == false)
            return false;
    }

    return true;
}
