#ifndef P1stream_conf_h
#define P1stream_conf_h

#include <stdbool.h>

typedef struct _P1Config P1Config;
typedef struct _P1ConfigSection P1ConfigSection; // opaque

typedef bool (*P1ConfigIterSection)(P1Config *cfg, P1ConfigSection *sect, void *data);
typedef bool (*P1ConfigIterString)(P1Config *cfg, const char *key, char *val, void *data);

struct _P1Config {
    void (*free)(P1Config *cfg);
    P1ConfigSection *(*get_section)(P1Config *cfg, P1ConfigSection *sect, const char *key);
    bool (*get_string)(P1Config *cfg, P1ConfigSection *sect, const char *key, char *buf, size_t bufsize);

    bool (*each_section)(P1Config *cfg, P1ConfigSection *sect, const char *key, P1ConfigIterSection iter, void *data);
    bool (*each_string)(P1Config *cfg, P1ConfigSection *sect, const char *key, P1ConfigIterString iter, void *data);
};

#endif
