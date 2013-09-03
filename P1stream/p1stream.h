#ifndef p1stream_h
#define p1stream_h

#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <pthread.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>

// Object types.
typedef struct _P1Config P1Config;
typedef void P1ConfigSection; // abstract
typedef enum _P1LogLevel P1LogLevel;
typedef enum _P1State P1State;
typedef enum _P1TargetState P1TargetState;
typedef struct _P1VideoClock P1VideoClock;
typedef struct _P1ListNode P1ListNode;
typedef struct _P1Source P1Source;
typedef struct _P1VideoSource P1VideoSource;
typedef struct _P1AudioSource P1AudioSource;
typedef struct _P1Video P1Video;
typedef struct _P1Audio P1Audio;
typedef struct _P1Connection P1Connection;
typedef struct _P1Context P1Context;
typedef enum _P1FreeOptions P1FreeOptions;
typedef enum _P1NotificationType P1NotificationType;
typedef enum _P1ObjectType P1ObjectType;
typedef struct _P1Notification P1Notification;

// Callback signatures.
typedef bool (*P1ConfigIterSection)(P1Config *cfg, P1ConfigSection *sect, void *data);
typedef bool (*P1ConfigIterString)(P1Config *cfg, const char *key, char *val, void *data);
typedef void (*P1LogCallback)(P1Context *ctx, P1LogLevel level, const char *fmt, va_list args, void *user_data);

// These types are for convenience. Sources usually want to have a function
// following one of these signatures to instantiate them.
typedef P1VideoClock *(P1VideoClockFactory)(P1Config *cfg, P1ConfigSection *sect);
typedef P1VideoSource *(P1VideoSourceFactory)(P1Config *cfg, P1ConfigSection *sect);
typedef P1AudioSource *(P1AudioSourceFactory)(P1Config *cfg, P1ConfigSection *sect);


// The interface below defines the set of operations used to read configuration.
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

    // The following methods return true if successful and false if undefined.
    // Unexpected types / values should be treated as undefined.

    // Copy a string value to the output buffer.
    bool (*get_string)(P1Config *cfg, P1ConfigSection *sect, const char *key, char *buf, size_t bufsize);
    // Read a float value.
    bool (*get_float)(P1Config *cfg, P1ConfigSection *sect, const char *key, float *out);
    // Read a boolean value.
    bool (*get_bool)(P1Config *cfg, P1ConfigSection *sect, const char *key, bool *out);

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

// This function should be used to change the state field on objects.
#define p1_set_state(_ctx, _object_type, _object, _state) {     \
    (_object)->state = (_state);                                \
    _p1_notify((_ctx), (P1Notification) {                       \
        .type = P1_NTYPE_STATE_CHANGE,                          \
        .object_type = (_object_type),                          \
        .object = (_object),                                    \
        .state_change = {                                       \
            .state = (_state)                                   \
        }                                                       \
    });                                                         \
}


// This is the state the source should be in, and should be worked towards.
enum _P1TargetState {
    P1_TARGET_RUNNING   = 0, // Initial value.
    P1_TARGET_IDLE      = 1,
    P1_TARGET_REMOVE    = 2  // Pending removal, source will be freed.
};

// This function should be used to change the target field on objects.
#define p1_set_target(_ctx, _object_type, _object, _target) {   \
    (_object)->target = (_target);                              \
    _p1_notify((_ctx), (P1Notification) {                       \
        .type = P1_NTYPE_TARGET_CHANGE,                         \
        .object_type = (_object_type),                          \
        .object = (_object),                                    \
        .target_change = {                                      \
            .target = (_target)                                 \
        }                                                       \
    });                                                         \
}


// Notifications are sent to the control thread so that it may track important
// changes that require it to take action.

// The same notification system is also used to update the user, which can be
// read from with p1_read. The user MUST read these notifications, or the
// control thread may eventually stall.

// Internally, the communication channel is backed by pipe(8), and buffers are
// large enough to make it difficult for actual stalling to occur, even if the
// users main thread is unable to read for seconds.

enum _P1NotificationType {
    P1_NTYPE_UNKNOWN        = 0,
    P1_NTYPE_STATE_CHANGE   = 1,
    P1_NTYPE_TARGET_CHANGE  = 2
};

enum _P1ObjectType {
    P1_OTYPE_UNKNOWN        = 0,
    P1_OTYPE_CONTEXT        = 1,
    P1_OTYPE_VIDEO          = 2,
    P1_OTYPE_AUDIO          = 3,
    P1_OTYPE_CONNECTION     = 4,
    P1_OTYPE_VIDEO_CLOCK    = 5,
    P1_OTYPE_VIDEO_SOURCE   = 6,
    P1_OTYPE_AUDIO_SOURCE   = 7
};

struct _P1Notification {
    P1NotificationType type;

    // Object that sent the notification.
    P1ObjectType object_type;
    void *object;

    // Content depends on the type field.
    union {

        struct {
            P1State state;
        } state_change;

        struct {
            P1TargetState target;
        } target_change;

    };
};


// Sources are tracked in circular linked lists. Each list has an empty head
// node. Helper methods are provided, but direct access is also possible.

struct _P1ListNode {
    P1ListNode *prev;
    P1ListNode *next;
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
// Inserting after the head node is basically a prepend.
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


// The video clock ticks at the video frame rate. The clock should start a
// thread and call back on p1_video_clock_tick. All video processing and
// encoding will happen on this thread.

struct _P1VideoClock {
    // Back reference, set automatically before start.
    P1Context *ctx;
    // Current state. The clock should update this with p1_set_state.
    P1State state;

    // The frame rate as a fraction. This should be set by the time the clock
    // goes into the running state.
    uint32_t fps_num;
    uint32_t fps_den;

    // Free the clock and associated resources. (Assume idle.)
    void (*free)(P1VideoClock *clock);

    // Start the clock. This should update the state and start the thread.
    bool (*start)(P1VideoClock *clock);
    // Stop the clock. This will only be called from the clocks own thread.
    void (*stop)(P1VideoClock *clock);
};

// Callback for video clocks to emit ticks.
void p1_video_clock_tick(P1VideoClock *vclock, int64_t time);


// Common interface for audio and video sources.

struct _P1Source {
    P1ListNode super;

    // Back reference, set automatically before start.
    P1Context *ctx;

    // Current state. The source should update this with p1_set_state.
    P1State state;
    // Target state. Change this with p1_set_target.
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

    // FIXME: separate compositor, and its data.

    // Texture name. The source need not touch this.
    GLuint texture;

    // The following coordinates are in the range [-1, +1].
    // They are pairs of bottom left and top right coordinates.

    // Top left and bottom right coordinates of where to place frames in the
    // output image. These are in the range [-1, +1].
    float x1, y1, x2, y2;
    // Top left and bottom right coordinates of the area in the frame to grab,
    // used to achieve clipping. These are in the range [0, 1].
    float u1, v1, u2, v2;

    // Produce the latest frame using p1_video_frame.
    // This is called from the clock thread.
    void (*frame)(P1VideoSource *source);
};

// Subclasses should call into this from the initializer.
void p1_video_source_init(P1VideoSource *vsrc, P1Config *cfg, P1ConfigSection *sect);

// Callback for video sources to provide frame data.
void p1_video_source_frame(P1VideoSource *vsrc, int width, int height, void *data);


// Audio sources produce buffers as they become available, using
// p1_audio_buffer. Several may be added to a context, to be mixed into a
// single output stream. Audio sources may emit buffers from any thread.

struct _P1AudioSource {
    P1Source super;

    // Mix buffer position. The source need not touch this.
    size_t mix_pos;

    // Whether this audio source is clock master.
    bool master;
    // In the range [0, 1].
    float volume;
};

// Subclasses should call into this from the initializer.
void p1_audio_source_init(P1AudioSource *asrc, P1Config *cfg, P1ConfigSection *sect);

// Callback for audio sources to provide audio buffer data.
void p1_audio_source_buffer(P1AudioSource *asrc, int64_t time, float *in, size_t samples);



// Audio mixer component.
struct _P1Audio {
    P1Context *ctx;

    // Current state.
    P1State state;
    // Target state. Change this with p1_set_target.
    P1TargetState target;

    // The source list. Can be modified while running, as long as the lock is
    // held. Use the p1_list_* functions for convenience.
    pthread_mutex_t lock;
    P1ListNode sources;
};


// Video mixer component.
struct _P1Video {
    P1Context *ctx;

    // Current state.
    P1State state;
    // Target state. Change this with p1_set_target.
    P1TargetState target;

    // The video clock. Only modify this when the video mixer is idle.
    P1VideoClock *clock;

    // The source list. Can be modified while running, as long as the lock is
    // held. Use the p1_list_* functions for convenience.
    pthread_mutex_t lock;
    P1ListNode sources;
};


// Stream connection component.
struct _P1Connection {
    P1Context *ctx;

    // Current state.
    P1State state;
    // Target state. Change this with p1_set_target.
    P1TargetState target;
};


// Log levels. These match x264s.
enum _P1LogLevel {
    P1_LOG_NONE     = -1,
    P1_LOG_ERROR    =  0,
    P1_LOG_WARNING  =  1,
    P1_LOG_INFO     =  2,
    P1_LOG_DEBUG    =  3
};

// Options for p1_free.
enum _P1FreeOptions {
    P1_FREE_ONLY_SELF       = 0,
    P1_FREE_VIDEO_CLOCK     = 1,
    P1_FREE_VIDEO_SOURCES   = 2,
    P1_FREE_AUDIO_SOURCES   = 4,
    P1_FREE_EVERYTHING      = 7
};

// Context that encapsulates everything else.
struct _P1Context {
    // Current state.
    P1State state;

    // Log function, defaults to stderr logging. Only modify this when the
    // context is idle. These can be called on any thread.
    P1LogCallback log_fn;
    void *log_user_data;
    // Maximum log level, defaults to P1_LOG_INFO.
    P1LogLevel log_level;

    // Fixed components.
    P1Video *video;
    P1Audio *audio;
    P1Connection *conn;
};

// Create a new context based on the given configuration.
P1Context *p1_create(P1Config *cfg, P1ConfigSection *sect);

// Free all resources related to the context, and optionally other objects.
void p1_free(P1Context *ctx, P1FreeOptions options);

// Start running with the current configuration.
void p1_start(P1Context *ctx);
// Stop all processing and all sources.
void p1_stop(P1Context *ctx);

// Read a P1Notification. This method will block.
void p1_read(P1Context *ctx, P1Notification *out);
// Returns a file descriptor that can be used with poll(2) or select(2),
// to determine if p1_read will not block on the next call.
int p1_fd(P1Context *ctx);

// Logging functions.
void p1_log(P1Context *ctx, P1LogLevel level, const char *fmt, ...) __printflike(3, 4);
void p1_logv(P1Context *ctx, P1LogLevel level, const char *fmt, va_list args) __printflike(3, 0);

// Low-level notification helper.
void _p1_notify(P1Context *ctx, P1Notification notification);


// Platform-specific functionality.
#if __APPLE__
#   include <TargetConditionals.h>
#   if TARGET_OS_MAC
#       include "osx/p1stream_osx.h"
#   endif
#endif

#endif
