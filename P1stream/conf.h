#ifndef P1stream_conf_h
#define P1stream_conf_h

#include <x264.h>

extern struct p1_conf_t {
    struct {
        char *url;
    } stream;

    x264_param_t encoder;
} p1_conf;

void p1_conf_init(const char *file);

#endif
