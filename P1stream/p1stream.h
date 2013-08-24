#ifndef p1stream_h
#define p1stream_h

#include <stdbool.h>
#include <stdint.h>

// Object types.
typedef struct _P1Config P1Config;
typedef void P1ConfigSection; // abstract
typedef enum _P1State P1State;
typedef enum _P1TargetState P1TargetState;
typedef struct _P1VideoClock P1VideoClock;
typedef struct _P1ListNode P1ListNode;
typedef struct _P1Source P1Source;
typedef struct _P1VideoSource P1VideoSource;
typedef struct _P1AudioSource P1AudioSource;
typedef struct _P1Context P1Context;
typedef enum _P1FreeOptions P1FreeOptions;

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


// Sources can be in one of the following states.
enum _P1State {
    P1StateIdle = 0, // Initial value.
    P1StateStarting = 1,
    P1StateRunning = 2,
    P1StateStopping = 3
};

// This is the state the source should be in, and should be worked towards.
enum _P1TargetState {
    P1TargetRunning, // Initial value.
    P1TargetIdle,
    P1TargetRemove  // Pending removal, source will be freed.
};


// Sources are tracked in circular linked lists. Each list has an empty head
// node. Helper methods are provided, but direct access is also possible.

struct _P1ListNode {
    P1ListNode *prev;
    P1ListNode *next;
};


// The video clock drives the main loop, which runs at the same rate as video
// output. The clock should start a thread and call back on p1_clock_tick.

// All state changes are handled on this thread, including state changes of
// audio sources, followed by video processing and encoding.

struct _P1VideoClock {
    // Back reference, set automatically before start.
    P1Context *ctx;
    // Current state.
    P1State state;

    // Free the source and associated resources. (Assume idle.)
    void (*free)(P1VideoClock *clock);

    // Start the clock. This should update the state and start the thread.
    bool (*start)(P1VideoClock *clock);
    // Stop the clock. This will only be called from the clocks own thread.
    void (*stop)(P1VideoClock *clock);
};


// Common interface for audio and video sources.

struct _P1Source {
    P1ListNode super;

    // Back reference, set automatically before start.
    P1Context *ctx;
    // Current state.
    P1State state;
    // Target state. P1stream will call start/stop/free accordingly.
    P1TargetState target;

    // Free the source and associated resources. (Assume idle.)
    void (*free)(P1Source *src);

    // Start the source. This should update the state and open resources.
    bool (*start)(P1Source *src);
    // Stop the source.
    void (*stop)(P1Source *src);
};


// Video sources produce images on each clock tick. Several may be added to a
// context, to be combined into a single output image.

struct _P1VideoSource {
    P1Source super;

    // Produce the latest frame using p1_video_frame.
    void (*frame)(P1VideoSource *source);
};


// Audio sources produce buffers as they become available, using
// p1_audio_buffer. Several may be added to a context, to be mixed into a
// single output stream. Audio sources may emit buffers from any thread.

struct _P1AudioSource {
    P1Source super;
};


// The main context containing all state.

struct _P1Context {
    // Current state.
    P1State state;

    // FIXME: add locks.
    P1VideoClock *clock;
    P1ListNode video_sources;
    P1ListNode audio_sources;
};


// Options for p1_free.
enum _P1FreeOptions {
    P1FreeOnlySelf = 0,
    P1FreeVideoClock = 1,
    P1FreeVideoSources = 2,
    P1FreeAudioSource = 4,
    P1FreeEverything = 7
};


// Low-level list manipulation helper.
#define _p1_list_manip(_src, _prev, _next) {        \
    _src->prev = _prev;                             \
    _src->next = _next;                             \
    _prev->next = _src;                             \
    _next->prev = _src;                             \
}

// Initialize a list.
#define p1_list_init(_head) {                       \
    P1ListNode *_p1_head = (P1ListNode *) (_head);  \
    _p1_head->prev = _p1_head;                      \
    _p1_head->next = _p1_head;                      \
}

// Insert a source node before the reference node.
// Inserting before the head node is basically an append.
#define p1_list_before(_ref, _node) {               \
    P1ListNode *_p1_node = (P1ListNode *) (_node);  \
    P1ListNode *_p1_next = (P1ListNode *) (_ref);   \
    P1ListNode *_p1_prev = _p1_next->prev;          \
    _p1_list_manip(_p1_node, _p1_prev, _p1_next);   \
}

// Insert a source node after the reference node.
// Inserting after the head node is basically a preprend.
#define p1_list_after(_ref, _node) {                \
    P1ListNode *_p1_node = (P1ListNode *) (_node);  \
    P1ListNode *_p1_prev = (P1ListNode *) (_ref);   \
    P1ListNode *_p1_next = _p1_prev->next;          \
    _p1_list_manip(_p1_node, _p1_prev, _p1_next);   \
}

// Remove a node from the list.
#define p1_list_remove(_node) {                     \
    P1ListNode *_p1_node = (P1ListNode *) (_node);  \
    P1ListNode *_p1_prev = _p1_node->prev;          \
    P1ListNode *_p1_next = _p1_node->next;          \
    _p1_prev->next = _p1_next;                      \
    _p1_next->prev = _p1_prev;                      \
}

// Iterate list items. Both arguments must be local variables.
#define p1_list_iterate(_head, _node)  \
    for (_node = _head->next; _node != _head; _node = _node->next)


// Create a new context based on the given configuration.
P1Context *p1_create(P1Config *cfg, P1ConfigSection *sect);

// Free all resources related to the context, and optionally other objects.
void p1_free(P1Context *ctx, P1FreeOptions options);

// Start running with the current configuration.
void p1_start(P1Context *ctx);
// Stop all processing and all sources.
void p1_stop(P1Context *ctx);

// Callback for video clocks to emit ticks.
void p1_clock_tick(P1VideoClock *src, int64_t time);
// Callback for video sources to provide frame data.
void p1_video_frame(P1VideoSource *src, int width, int height, void *data);
// Callback for audio sources to provide audio buffer data.
void p1_audio_buffer(P1AudioSource *dtv, int64_t time, void *in, int in_len);


// Platform-specific functionality.
#if __APPLE__
#   include <TargetConditionals.h>
#   if TARGET_OS_MAC
#       include "osx/p1stream_osx.h"
#   endif
#endif

#endif
