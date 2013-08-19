#ifndef p1_cli_audio_h
#define p1_cli_audio_h

#include <stdbool.h>
#include <stdint.h>


// Fixed internal mixing buffer parameters.
static const int p1_audio_sample_rate = 44100;
static const int p1_audio_sample_size = 2;
static const int p1_audio_num_channels = 2;


typedef struct _P1AudioSource P1AudioSource;
typedef struct _P1AudioSourceFactory P1AudioSourceFactory;

struct _P1AudioSource {
    P1AudioSourceFactory *factory;
    void (*free)(P1AudioSource *src);

    bool (*start)(P1AudioSource *src);
    void (*stop)(P1AudioSource *src);
};

struct _P1AudioSourceFactory {
    P1AudioSource *(*create)();
};


void p1_audio_init();
void p1_audio_add_source(P1AudioSource *src);
void p1_audio_mix(P1AudioSource *dtv, int64_t time, void *in, int in_len);

#endif
