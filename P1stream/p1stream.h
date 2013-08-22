#ifndef p1stream_h
#define p1stream_h

#include <stdbool.h>
#include <stdint.h>

// Object types.
typedef struct _P1Config P1Config;
typedef void P1ConfigSection; // abstract
typedef struct _P1VideoClock P1VideoClock;
typedef struct _P1VideoSource P1VideoSource;
typedef struct _P1AudioSource P1AudioSource;
typedef struct _P1Context P1Context;

// Callback signatures.
typedef bool (*P1ConfigIterSection)(P1Config *cfg, P1ConfigSection *sect, void *data);
typedef bool (*P1ConfigIterString)(P1Config *cfg, const char *key, char *val, void *data);

// These types are for convenience. Sources usually want to have a function
// following one of these signatures to instantiate them.
typedef P1AudioSource *(P1AudioSourceFactory)(P1Config *cfg, P1ConfigSection *sect);
typedef P1VideoClock *(P1VideoClockFactory)(P1Config *cfg, P1ConfigSection *sect);
typedef P1VideoSource *(P1VideoSourceFactory)(P1Config *cfg, P1ConfigSection *sect);


// The interfaces below define the set of operations used to read configuration.
// This should be simple enough to allow backing by a variety of stores like a
// JSON file, property list file, or registry.

// Keys are strings that may identify an item several levels deep, separated by
// periods. Each level is called a section, and would for example be an object
// in JSON. Though it's also possible to have a flat store and treat keys as
// paths, properly concatenating where the interface requires it.

struct _P1Config {
    // Free resources.
    void (*free)(P1Config *cfg);

    // Get a reference to the section. If this returns NULL, all items expected
    // to be inside should be treated as undefined.
    P1ConfigSection *(*get_section)(P1Config *cfg, P1ConfigSection *sect, const char *key);
    // Copy a string value to the output buffer. True if successful, false if
    // undefined. Unexpected types / values should be treated as undefined.
    bool (*get_string)(P1Config *cfg, P1ConfigSection *sect, const char *key, char *buf, size_t bufsize);

    // Iterate sections in an array, used to gather sources.
    bool (*each_section)(P1Config *cfg, P1ConfigSection *sect, const char *key, P1ConfigIterSection iter, void *data);
    // Iterate keys and string values in a section.
    bool (*each_string)(P1Config *cfg, P1ConfigSection *sect, const char *key, P1ConfigIterString iter, void *data);
};


// Video sources may be added, removed and rearranged at run-time, but a stable
// clock is needed to produce output with a constant frame rate. This is the
// interface we expect such a clock implementation to provide.

struct _P1VideoClock {
    // Back reference, set on p1_video_set_clock.
    P1Context *ctx;

    // Free the source and associated resources. (Assume already stopped.)
    void (*free)(P1VideoClock *clock);
    // Start the clock. Emit ticks using p1_video_clock_tick.
    bool (*start)(P1VideoClock *clock);
    // Stop the clock.
    void (*stop)(P1VideoClock *clock);
};


// Video sources produce images on each clock tick. Several may be added to a
// context, to be combined into a single output image.

struct _P1VideoSource {
    // Back reference, set on p1_video_add_source.
    P1Context *ctx;

    // Free the source and associated resources. (Assume already stopped.)
    void (*free)(P1VideoSource *source);
    // Start the source. (Ie. open resources, start reading.)
    bool (*start)(P1VideoSource *source);
    // Produce the latest frames using p1_video_frame.
    void (*frame)(P1VideoSource *source);
    // Stop the source.
    void (*stop)(P1VideoSource *source);
};


// Audio sources produce buffers as they become available. Several may be added
// to a context, to be mixed into a single output stream.

struct _P1AudioSource {
    // Back reference, set on p1_audio_add_source.
    P1Context *ctx;

    // Free the source and associated resources. (Assume already stopped.)
    void (*free)(P1AudioSource *src);
    // Start the source. (Ie. open resources, start reading.)
    // Produce buffers using p1_audio_mix.
    bool (*start)(P1AudioSource *src);
    // Stop the source.
    void (*stop)(P1AudioSource *src);
};


// Create a new context based on the given configuration.
P1Context *p1_create(P1Config *cfg, P1ConfigSection *sect);

void p1_video_set_clock(P1Context *ctx, P1VideoClock *clock);
void p1_video_add_source(P1Context *ctx, P1VideoSource *src);

void p1_video_clock_tick(P1VideoClock *src, int64_t time);
void p1_video_frame(P1VideoSource *src, int width, int height, void *data);

void p1_audio_add_source(P1Context *ctx, P1AudioSource *src);
void p1_audio_mix(P1AudioSource *dtv, int64_t time, void *in, int in_len);


// Platform-specific functionality.
#if __APPLE__
#   include <TargetConditionals.h>
#   if TARGET_OS_MAC
#       include "osx/p1stream_osx.h"
#   endif
#endif

#endif
