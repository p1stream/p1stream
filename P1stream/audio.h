#ifndef p1_cli_audio_h
#define p1_cli_audio_h

#include <stdint.h>

// Fixed internal mixing buffer parameters.
static const int p1_audio_sample_rate = 44100;
static const int p1_audio_sample_size = 2;
static const int p1_audio_num_channels = 2;

void p1_audio_init();
void p1_audio_mix(int64_t time, void *in, int in_len);

#endif
