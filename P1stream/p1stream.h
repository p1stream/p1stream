#ifndef p1stream_h
#define p1stream_h

#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>

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
typedef enum _P1NotificationType P1NotificationType;
typedef enum _P1ObjectType P1ObjectType;
typedef struct _P1Notification P1Notification;

// Callback signatures.
typedef bool (*P1ConfigIterSection)(P1Config *cfg, P1ConfigSection *sect, void *data);
typedef bool (*P1ConfigIterString)(P1Config *cfg, const char *key, char *val, void *data);

// These types are for convenience. Sources usually want to have a function
// following one of these signatures to instantiate them.
typedef P1VideoClock *(P1VideoClockFactory)(P1Config *cfg, P1ConfigSection *sect);
typedef P1VideoSource *(P1VideoSourceFactory)(P1Config *cfg, P1ConfigSection *sect);
typedef P1AudioSource *(P1AudioSourceFactory)(P1Config *cfg, P1ConfigSection *sect);


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
    P1_STATE_IDLE       = 0, // Initial value.
    P1_STATE_STARTING   = 1,
    P1_STATE_RUNNING    = 2,
    P1_STATE_STOPPING   = 3
};

// This is the state the source should be in, and should be worked towards.
enum _P1TargetState {
    P1_TARGET_RUNNING   = 0, // Initial value.
    P1_TARGET_IDLE      = 1,
    P1_TARGET_REMOVE    = 2  // Pending removal, source will be freed.
};


// Sources are tracked in circular linked lists. Each list has an empty head
// node. Helper methods are provided, but direct access is also possible.

struct _P1ListNode {
    P1ListNode *prev;
    P1ListNode *next;
};


// The video clock runs at the video frame rate. The clock should start a
// thread and call back on p1_video_clock_tick. All video processing and
// encoding will happen on this thread.

struct _P1VideoClock {
    // Back reference, set automatically before start.
    P1Context *ctx;
    // Current state.
    P1State state;

    // Free the clock and associated resources. (Assume idle.)
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
    pthread_mutex_t lock;

    // Current state.
    P1State state;

    // FIXME: add locks.
    P1VideoClock *clock;
    P1ListNode video_sources;
    P1ListNode audio_sources;
};


// Options for p1_free.
enum _P1FreeOptions {
    P1_FREE_ONLY_SELF       = 0,
    P1_FREE_VIDEO_CLOCK     = 1,
    P1_FREE_VIDEO_SOURCES   = 2,
    P1_FREE_AUDIO_SOURCES   = 4,
    P1_FREE_EVERYTHING      = 7
};


// These are types used to communicate with the control thread.

enum _P1NotificationType {
    P1_NTYPE_UNKNOWN        = 0,
    P1_NTYPE_STATE_CHANGE   = 1,
    P1_NTYPE_TARGET_CHANGE  = 2
};

enum _P1ObjectType {
    P1_OTYPE_UNKNOWN        = 0,
    P1_OTYPE_CONTEXT        = 1,
    P1_OTYPE_VIDEO_CLOCK    = 2,
    P1_OTYPE_VIDEO_SOURCE   = 3,
    P1_OTYPE_AUDIO_SOURCE   = 4
};

struct _P1Notification {
    P1NotificationType type;
    P1ObjectType object_type;
    void *object;
    union {
        struct {
            P1State state;
        } state_change;
        struct {
            P1TargetState target;
        } target_change;
    };
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

// Read a P1StateNotification. This method will block.
// The user must read these notifications.
void p1_read(P1Context *ctx, P1Notification *out);
// Returns a file descriptor that can be used with poll(2) or select(2),
// to determine if p1_read will not block on the next call.
int p1_fd(P1Context *ctx);

// This function should be used to change the state field on objects.
#define p1_set_state(_ctx, _object_type, _object, _state) {     \
    (_object)->state = (_state);                                \
    p1_notify((_ctx), (P1Notification) {                        \
        .type = P1_NTYPE_STATE_CHANGE,                          \
        .object_type = (_object_type),                          \
        .object = (_object),                                    \
        .state_change = {                                       \
            .state = (_state)                                   \
        }                                                       \
    });                                                         \
}
// This function should be used to change the target field on objects.
#define p1_set_target(_ctx, _object_type, _object, _target) {   \
    (_object)->target = (_target);                              \
    p1_notify((_ctx), (P1Notification) {                        \
        .type = P1_NTYPE_TARGET_CHANGE,                         \
        .object_type = (_object_type),                          \
        .object = (_object),                                    \
        .target_change = {                                      \
            .target = (_target)                                 \
        }                                                       \
    });                                                         \
}
// Helper used to send notifications.
void p1_notify(P1Context *ctx, P1Notification notification);

// Callback for video clocks to emit ticks.
void p1_video_clock_tick(P1VideoClock *vclock, int64_t time);
// Callback for video sources to provide frame data.
void p1_video_frame(P1VideoSource *vsrc, int width, int height, void *data);
// Callback for audio sources to provide audio buffer data.
void p1_audio_buffer(P1AudioSource *asrc, int64_t time, void *in, int in_len);


// Platform-specific functionality.
#if __APPLE__
#   include <TargetConditionals.h>
#   if TARGET_OS_MAC
#       include "osx/p1stream_osx.h"
#   endif
#endif

#endif
