#ifndef p1stream_h
#define p1stream_h

#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <pthread.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>


// The P1stream interface consist of a context that models a simple media
// pipeline containing elements, each taking responsibility for part of the
// media processing.
//
// There are three fixed elements in each context:
//
//  - An instance of P1Video that assembles the video frames, does colorspace
//    conversion and encoding to H.264.
//
//  - An instance of P1Audio that mixes audio buffers, and encodes to AAC.
//
//  - An instance of P1Connection that manages the RTMP streaming connection,
//    muxing of the streams and buffering.
//
// The remaining elements are plugins provided by the user:
//
//  - A single instance of a P1VideoClock subclass that provides video timing.
//  - Any number of instances of P1VideoSources subclasses.
//  - Any number of instances of P1AudioSources subclasses.
//
// P1stream bundles several plugins for the most common tasks, but the user is
// free to implement their own.


// Object types.
typedef struct _P1Config P1Config;
typedef struct _P1Element P1Element;
typedef struct _P1PluginElement P1PluginElement;
typedef struct _P1Source P1Source;
typedef struct _P1VideoClock P1VideoClock;
typedef struct _P1VideoSource P1VideoSource;
typedef struct _P1AudioSource P1AudioSource;
typedef struct _P1Video P1Video;
typedef struct _P1Audio P1Audio;
typedef struct _P1Connection P1Connection;
typedef struct _P1Context P1Context;

// Misc. types.
typedef enum _P1LogLevel P1LogLevel;
typedef enum _P1State P1State;
typedef enum _P1TargetState P1TargetState;
typedef enum _P1FreeOptions P1FreeOptions;
typedef enum _P1NotificationType P1NotificationType;
typedef enum _P1ObjectType P1ObjectType;
typedef struct _P1ListNode P1ListNode;
typedef struct _P1Notification P1Notification;
typedef void P1ConfigSection; // abstract

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


// Elements track simple state. These are the possible states.

enum _P1State {
    P1_STATE_IDLE       = 0, // Initial value.
    P1_STATE_STARTING   = 1,
    P1_STATE_RUNNING    = 2,
    P1_STATE_STOPPING   = 3
};

// This function should be used to change the state field on elements.
#define p1_set_state(_el, _type, _state) {                      \
    P1Element *_p1_el = (_el);                                  \
    P1State _p1_state = (_state);                               \
    _p1_el->state = _p1_state;                                  \
    _p1_notify(_p1_el->ctx, (P1Notification) {                  \
        .type = P1_NTYPE_STATE_CHANGE,                          \
        .object_type = (_type),                                 \
        .object = _p1_el,                                       \
        .state_change = {                                       \
            .state = _p1_state                                  \
        }                                                       \
    });                                                         \
}


// This is the state we want an element to be in, and should be worked towards.

enum _P1TargetState {
    P1_TARGET_RUNNING   = 0, // Initial value.
    P1_TARGET_IDLE      = 1,
    P1_TARGET_REMOVE    = 2  // Pending removal, source will be freed.
};

// This function should be used to change the target field on elements.
#define p1_set_target(_el, _type, _target) {                    \
    P1Element *_p1_el = (_el);                                  \
    P1TargetState _p1_target = (_target);                       \
    _p1_el->target = _p1_target;                                \
    _p1_notify((_ctx), (P1Notification) {                       \
        .type = P1_NTYPE_TARGET_CHANGE,                         \
        .object_type = (_type),                                 \
        .object = _p1_el,                                       \
        .target_change = {                                      \
            .target = _p1_target                                \
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

// Initialize a list.
#define p1_list_init(_head) {                       \
    P1ListNode *_p1_head = (_head);                 \
    _p1_head->prev = _p1_head;                      \
    _p1_head->next = _p1_head;                      \
}

// Get the struct containing this node.
#define p1_list_get_container(_node, _type, _field) \
    (_type *) ((void *) (_node) - offsetof(_type, _field))

// Insert a source node before the reference node.
// Inserting before the head node is basically an append.
#define p1_list_before(_ref, _node)                 \
    P1ListNode *_p1_ref = (_ref);                   \
    P1ListNode *_p1_node = (_node);                 \
    _p1_list_between(_p1_ref->prev, _p1_ref, _p1_node)

// Insert a source node after the reference node.
// Inserting after the head node is basically a prepend.
#define p1_list_after(_ref, _node)                  \
    P1ListNode *_p1_ref = (_ref);                   \
    P1ListNode *_p1_node = (_node);                 \
    _p1_list_between(_p1_ref, _p1_ref->next, _p1_node)

// Remove a node from the list.
#define p1_list_remove(_node) {                     \
    P1ListNode *_p1_node = (_node);                 \
    P1ListNode *_p1_prev = _p1_node->prev;          \
    P1ListNode *_p1_next = _p1_node->next;          \
    _p1_prev->next = _p1_next;                      \
    _p1_next->prev = _p1_prev;                      \
}

// Iterate list items. Both arguments must be local variables.
#define p1_list_iterate(_head, _node)  \
    for (_node = _head->next; _node != _head; _node = _node->next)

// List manipulation helper.
#define _p1_list_between(_prev, _next, _src) {      \
    _src->prev = _prev;                             \
    _src->next = _next;                             \
    _prev->next = _src;                             \
    _next->prev = _src;                             \
}


// Base of all elements that live in a context.

struct _P1Element {
    // Back reference. This will be set automatically.
    P1Context *ctx;

    // All operations on an element should be done while its lock is held.
    // (Certain exceptions are possible, e.g. the source or context is idle.)
    pthread_mutex_t lock;

    // Current state. Only the element itself should update this field, and do
    // so with p1_set_state.
    P1State state;
    // Target state. This field can be updated with p1_set_target.
    P1TargetState target;
};

// Convenience methods for acquiring the element lock.
#define p1_element_lock(_el) assert(pthread_mutex_lock(&(_el)->lock) == 0)
#define p1_element_unlock(_el) assert(pthread_mutex_unlock(&(_el)->lock) == 0)


// Base for all plugin (non-fixed) elements.

struct _P1PluginElement {
    P1Element super;

    // Free the object and associated resources. (Assume idle.)
    // Implementation is optional. If NULL, a regular free() is used instead.
    void (*free)(P1PluginElement *pel);

    // Start the source. This should update the state and open resources.
    bool (*start)(P1PluginElement *pel);
    // Stop the source. This should update the state and close resources.
    void (*stop)(P1PluginElement *pel);
};

// Call this to free a plugin element. This is rarely needed. Instead, set the
// target state to remove, or free it on context destruction with p1_free.
void p1_plugin_element_free(P1PluginElement *obj);


// Base for audio and video sources.

struct _P1Source {
    P1PluginElement super;

    // Link in the source list.
    P1ListNode link;
};


// The video clock ticks at the video frame rate. The clock should start a
// thread and call back on p1_video_clock_tick. All video processing and
// encoding will happen on this thread.

struct _P1VideoClock {
    P1PluginElement super;

    // The frame rate as a fraction. This should be set by the time the clock
    // goes into the running state.
    uint32_t fps_num;
    uint32_t fps_den;
};

// Subclasses should call into this from the initializer.
void p1_video_clock_init(P1VideoClock *vclock, P1Config *cfg, P1ConfigSection *sect);

// Callback for video clocks to emit ticks.
void p1_video_clock_tick(P1VideoClock *vclock, int64_t time);


// Video sources produce images on each clock tick. Several may be added to a
// context, to be combined into a single output image.

struct _P1VideoSource {
    P1Source super;

    // FIXME: separate compositor, and its data.

    // Texture name. The source need not touch this.
    GLuint texture;

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


// Fixed audio mixer element.

struct _P1Audio {
    P1Element super;

    // The source list. Can be modified while running, as long as the lock is
    // held. Use the p1_list_* functions for convenience.
    P1ListNode sources;
};


// Fixed video mixer element.

struct _P1Video {
    P1Element super;

    // The video clock. Only modify this when the video mixer is idle.
    P1VideoClock *clock;

    // The source list. Can be modified while running, as long as the lock is
    // held. Use the p1_list_* functions for convenience.
    P1ListNode sources;
};


// Fixed stream connection element.

struct _P1Connection {
    P1Element super;
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

// Notification helper.
void _p1_notify(P1Context *ctx, P1Notification notification);


// Platform-specific functionality.

#if __APPLE__
#   include <TargetConditionals.h>
#   if TARGET_OS_MAC
#       include "osx/p1stream_osx.h"
#   endif
#endif

#endif
