#include "p1stream.h"

#include <string.h>


typedef struct _P1PlistConfig P1PlistConfig;

struct _P1PlistConfig {
    P1Config super;

    CFDictionaryRef dict;
};

static void p1_plist_config_free(P1Config *_cfg);
static NSObject *p1_plist_config_resolve(P1Config *cfg, const char *key, Class type);
static bool p1_plist_config_get_string(P1Config *cfg, const char *key, char *buf, size_t bufsize);
static bool p1_plist_config_get_int(P1Config *cfg, const char *key, int *out);
static bool p1_plist_config_get_float(P1Config *cfg, const char *key, float *out);
static bool p1_plist_config_get_bool(P1Config *cfg, const char *key, bool *out);
static bool p1_plist_config_each_string(P1Config *cfg, const char *prefix, P1ConfigIterString iter, void *data);


P1Config *p1_plist_config_create(NSDictionary *dict)
{
    P1PlistConfig *pcfg = calloc(1, sizeof(P1PlistConfig));
    if (pcfg == NULL) {
        p1_log(NULL, P1_LOG_ERROR, "Failed to allocate property list config object");
        return NULL;
    }

    pcfg->dict = CFBridgingRetain(dict);

    P1Config *cfg = (P1Config *) pcfg;
    cfg->free        = p1_plist_config_free;
    cfg->get_string  = p1_plist_config_get_string;
    cfg->get_int     = p1_plist_config_get_int;
    cfg->get_float   = p1_plist_config_get_float;
    cfg->get_bool    = p1_plist_config_get_bool;
    cfg->each_string = p1_plist_config_each_string;
    return cfg;
}

static void p1_plist_config_free(P1Config *cfg)
{
    P1PlistConfig *pcfg = (P1PlistConfig *) cfg;

    CFRelease(pcfg->dict);
    free(pcfg);
}

// Internal helper. Always called within an autoreleasepool.
static NSObject *p1_plist_config_get(P1Config *cfg, const char *key, Class type)
{
    P1PlistConfig *pcfg = (P1PlistConfig *) cfg;
    NSDictionary *dict = (__bridge NSDictionary *) pcfg->dict;

    NSString *ns_key = [NSString stringWithUTF8String:key];
    NSObject *val = dict[ns_key];
    if (!val)
        return nil;

    if (![val isKindOfClass:type]) {
        p1_log(NULL, P1_LOG_ERROR, "Invalid type for '%s', treating as undefined", key);
        return nil;
    }

    return val;
}

static bool p1_plist_config_get_string(P1Config *cfg, const char *key, char *buf, size_t bufsize)
{
    @autoreleasepool {
        NSString *val = (NSString *) p1_plist_config_get(cfg, key, [NSString class]);
        if (val)
            return [val getCString:buf maxLength:bufsize encoding:NSUTF8StringEncoding];
        else
            return false;
    }
}

static bool p1_plist_config_get_int(P1Config *cfg, const char *key, int *out)
{
    @autoreleasepool {
        NSNumber *val = (NSNumber *) p1_plist_config_get(cfg, key, [NSNumber class]);
        if (val) {
            *out = [val intValue];
            return true;
        }
        else {
            return false;
        }
    }
}

static bool p1_plist_config_get_float(P1Config *cfg, const char *key, float *out)
{
    @autoreleasepool {
        NSNumber *val = (NSNumber *) p1_plist_config_get(cfg, key, [NSNumber class]);
        if (val) {
            *out = [val floatValue];
            return true;
        }
        else {
            return false;
        }
    }
}

static bool p1_plist_config_get_bool(P1Config *cfg, const char *key, bool *out)
{
    @autoreleasepool {
        NSNumber *val = (NSNumber *) p1_plist_config_get(cfg, key, [NSNumber class]);
        if (val) {
            *out = [val boolValue];
            return true;
        }
        else {
            return false;
        }
    }
}

static bool p1_plist_config_each_string(P1Config *cfg, const char *prefix, P1ConfigIterString iter, void *data)
{
    @autoreleasepool {
        P1PlistConfig *pcfg = (P1PlistConfig *) cfg;
        NSDictionary *dict = (__bridge NSDictionary *) pcfg->dict;

        NSString *ns_prefix = [NSString stringWithUTF8String:prefix];
        for (id nsKeyId in dict) {
            if (![nsKeyId isKindOfClass:[NSString class]])
                continue;

            NSString *ns_key = nsKeyId;
            if (![ns_key hasPrefix:ns_prefix])
                continue;

            NSString *ns_val = dict[ns_key];
            const char *key = [ns_key cStringUsingEncoding:NSUTF8StringEncoding];
            const char *val = [ns_val cStringUsingEncoding:NSUTF8StringEncoding];
            if (!iter(cfg, key, val, data))
                return false;
        }
    }
    return true;
}
