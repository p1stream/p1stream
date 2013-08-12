#ifndef P1stream_conf_h
#define P1stream_conf_h

extern struct p1_conf_t {
    struct {
        char *url;
    } stream;

    struct {
        char *preset;
        char *profile;
    } encoder;
} p1_conf;

void p1_conf_init(const char *file);

#endif
