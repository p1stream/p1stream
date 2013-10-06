#ifndef p1stream_h
#define p1stream_h

#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <pthread.h>


// The P1stream interface consist of a context that models a simple media
// pipeline containing elements, each taking responsibility for part of the
// media processing.
//
// There are three fixed elements in each context:
//
//  - An instance of P1Video that mixes video frames into a single output image.
//
//  - An instance of P1Audio that mixes audio buffers.
//
//  - An instance of P1Connection encoding and RTMP streaming.
//
// The remaining elements are plugins provided by the user:
//
//  - A single instance of a P1VideoClock subclass that provides video timing.
//
//  - Any number of instances of P1VideoSource subclasses.
//
//  - Any number of instances of P1AudioSource subclasses.
//
// P1stream bundles several plugins for the most common tasks, but the user is
// free to implement their own.


// Object types.
typedef struct _P1Config P1Config;
typedef struct _P1Object P1Object;
typedef struct _P1Plugin P1Plugin;
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
typedef enum _P1StopOptions P1StopOptions;
typedef enum _P1FreeOptions P1FreeOptions;
typedef enum _P1State P1State;
typedef enum _P1TargetState P1TargetState;
typedef enum _P1NotificationType P1NotificationType;
typedef enum _P1ObjectType P1ObjectType;
typedef struct _P1ListNode P1ListNode;
typedef struct _P1Notification P1Notification;

// Callback signatures.
typedef bool (*P1ConfigIterString)(P1Config *cfg, const char *key, const char *val, void *data);
typedef void (*P1LogCallback)(P1Object *obj, P1LogLevel level, const char *fmt, va_list args, void *user_data);
typedef void (*P1VideoPreviewCallback)(size_t width, size_t height, uint8_t *data, void *user_data);

// These types are for convenience. Sources usually want to have a function
// following one of these signatures to instantiate them.
typedef P1VideoClock *(P1VideoClockFactory)();
typedef P1VideoSource *(P1VideoSourceFactory)();
typedef P1AudioSource *(P1AudioSourceFactory)();


// Log levels. These match x264s.

enum _P1LogLevel {
    P1_LOG_NONE     = -1,
    P1_LOG_ERROR    =  0,
    P1_LOG_WARNING  =  1,
    P1_LOG_INFO     =  2,
    P1_LOG_DEBUG    =  3
};


// Options for p1_stop.

enum _P1StopOptions {
    P1_STOP_ASYNC           = 0,
    P1_STOP_SYNC            = 1
};


// Options for p1_free.

enum _P1FreeOptions {
    P1_FREE_ONLY_SELF       = 0,
    P1_FREE_VIDEO_CLOCK     = 1,
    P1_FREE_VIDEO_SOURCES   = 2,
    P1_FREE_AUDIO_SOURCES   = 4,
    P1_FREE_EVERYTHING      = 7
};


// Objects track simple state. These are the possible states.

enum _P1State {
    P1_STATE_IDLE       = 0, // Initial value.
    P1_STATE_STARTING   = 1,
    P1_STATE_RUNNING    = 2,
    P1_STATE_STOPPING   = 3,

    // These are the same as stopping / idle, but for unexpected situations.
    // No further action is taken until the situation is resolved with
    // p1_object_clear_halt.
    P1_STATE_HALTING    = 4,
    P1_STATE_HALTED     = 5
};


// This is the state we want an object to be in, and should be worked towards.

enum _P1TargetState {
    P1_TARGET_RUNNING   = 0, // Initial value.
    P1_TARGET_IDLE      = 1,

    // Only valid for sources. This is the same as idle, but will in addition
    // remove the source from the list and free it, once idle.
    P1_TARGET_REMOVE    = 2
};


// Notification types.

enum _P1NotificationType {
    P1_NTYPE_NULL           = 0,
    P1_NTYPE_STATE_CHANGE   = 1,
    P1_NTYPE_TARGET_CHANGE  = 2
};


// Object types.

enum _P1ObjectType {
    P1_OTYPE_CONTEXT        = 1,
    P1_OTYPE_VIDEO          = 2,
    P1_OTYPE_AUDIO          = 3,
    P1_OTYPE_CONNECTION     = 4,
    P1_OTYPE_VIDEO_CLOCK    = 5,
    P1_OTYPE_VIDEO_SOURCE   = 6,
    P1_OTYPE_AUDIO_SOURCE   = 7
};


// Lock utility functions

#define p1_lock(_obj, _mutex) ({                                \
    int _p1_ret = pthread_mutex_lock(_mutex);                   \
    if (_p1_ret != 0)                                           \
        p1_log((_obj), P1_LOG_ERROR, "Failed to acquire lock: %s", strerror(_p1_ret));  \
})

#define p1_unlock(_obj, _mutex) ({                              \
    int _p1_ret = pthread_mutex_unlock(_mutex);                 \
    if (_p1_ret != 0)                                           \
        p1_log((_obj), P1_LOG_ERROR, "Failed to release lock: %s", strerror(_p1_ret));  \
})


// The interface below defines the set of operations used to read configuration.
// This should be simple enough to allow backing by a variety of stores like a
// JSON file, property list file, or registry.

struct _P1Config {
    // Free resources.
    void (*free)(P1Config *cfg);

    // The following methods return true if successful and false if undefined.
    // Unexpected types should also be treated as undefined.

    // Copy a string value to the output buffer.
    bool (*get_string)(P1Config *cfg, const char *key, char *buf, size_t bufsize);
    // Read an integer value.
    bool (*get_int)(P1Config *cfg, const char *key, int *out);
    // Read a float value.
    bool (*get_float)(P1Config *cfg, const char *key, float *out);
    // Read a boolean value.
    bool (*get_bool)(P1Config *cfg, const char *key, bool *out);

    // Iterate keys and string values with the given prefix.
    bool (*each_string)(P1Config *cfg, const char *prefix, P1ConfigIterString iter, void *data);
};

// Free a config object.
#define p1_config_free(_cfg) {                                  \
    P1Config *_p1_cfg = (_cfg);                                 \
    _p1_cfg->free(_p1_cfg);                                     \
}


// Notifications are sent to the control thread so that it may track important
// changes that require it to take action.

// The same notification system is also used to update the user, which can be
// read from with p1_read. The user MUST read these notifications, or the
// control thread may eventually stall.

// Internally, the communication channel is backed by pipe(8), and buffers are
// large enough to make it difficult for actual stalling to occur, even if the
// users main thread is unable to read for seconds.

struct _P1Notification {
    P1NotificationType type;

    // Object that sent the notification.
    P1Object *object;

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


// Base of all objects that live in a context.

struct _P1Object {
    // Back reference. This will be set automatically.
    P1Context *ctx;

    // Basic type of the object.
    P1ObjectType type;

    // All operations on an element should be done while its lock is held.
    // (Certain exceptions are possible, e.g. the source or context is idle.)
    pthread_mutex_t lock;

    // Current state. Only the element itself should update this field, and do
    // so with p1_set_state.
    P1State state;
    // Target state. This field can be updated with p1_set_target.
    P1TargetState target;

    // Anything the user may want to associate with this object.
    void *user_data;
};

// Convenience methods for acquiring the object lock.
#define p1_object_lock(_obj) ({                                 \
    P1Object *_p1_obj = (_obj);                                 \
    p1_lock(_p1_obj, &_p1_obj->lock);                           \
})

#define p1_object_unlock(_obj) ({                               \
    P1Object *_p1_obj = (_obj);                                 \
    p1_unlock(_p1_obj, &_p1_obj->lock);                         \
})

// This method should be used to change the state field.
#define p1_object_set_state(_obj, _state) ({                    \
    P1Object *_p1_obj = (_obj);                                 \
    P1State _p1_state = (_state);                               \
    bool _p1_changed = (_p1_obj->state != _p1_state);           \
    if (_p1_changed) {                                          \
        _p1_obj->state = _p1_state;                             \
        _p1_notify((P1Notification) {                           \
            .type = P1_NTYPE_STATE_CHANGE,                      \
            .object = _p1_obj,                                  \
            .state_change = {                                   \
                .state = _p1_state                              \
            }                                                   \
        });                                                     \
    }                                                           \
    _p1_changed;                                                \
})

// This method should be used to change the target field.
#define p1_object_set_target(_obj, _target) ({                  \
    P1Object *_p1_obj = (_obj);                                 \
    P1TargetState _p1_target = (_target);                       \
    bool _p1_changed = (_p1_obj->target != _p1_target);         \
    if (_p1_changed) {                                          \
        _p1_obj->target = _p1_target;                           \
        _p1_notify((P1Notification) {                           \
            .type = P1_NTYPE_TARGET_CHANGE,                     \
            .object = _p1_obj,                                  \
            .target_change = {                                  \
                .target = _p1_target                            \
            }                                                   \
        });                                                     \
    }                                                           \
    _p1_changed;                                                \
})

// Clear a situation that has caused the object to go into a halt state.
#define p1_object_clear_halt(_obj) ({                           \
    P1Object *_p1_objx = (_obj);                                \
    bool _p1_is_halted = (_p1_objx->state == P1_STATE_HALTED);  \
    if (_p1_is_halted)                                          \
        p1_object_set_state(_p1_objx, P1_STATE_IDLE);           \
    _p1_is_halted;                                              \
})


// Base for all plugin (non-fixed) elements.

struct _P1Plugin {
    P1Object super;

    // Read / load configuration. Implementation is optional.
    void (*config)(P1Plugin *pel, P1Config *cfg);

    // Free the object and associated resources. (Assume idle.)
    // Implementation is optional. If NULL, a regular free() is used instead.
    void (*free)(P1Plugin *pel);

    // Start the source. This should update the state and open resources.
    void (*start)(P1Plugin *pel);
    // Stop the source. This should update the state and close resources.
    void (*stop)(P1Plugin *pel);
};

// Call this to free a plugin element. This is rarely needed. Instead, set the
// target state to remove, or free it on context destruction with p1_free.
void p1_plugin_free(P1Plugin *obj);


// Base for audio and video sources.

struct _P1Source {
    P1Plugin super;

    // Link in the source list.
    P1ListNode link;
};


// The video clock ticks at the video frame rate. The clock should start a
// thread and call back on p1_video_clock_tick. All video processing and
// encoding will happen on this thread.

struct _P1VideoClock {
    P1Plugin super;

    // The frame rate as a fraction. This should be set by the time the clock
    // goes into the running state.
    uint32_t fps_num;
    uint32_t fps_den;
};

// Subclasses should call this from the initializer.
bool p1_video_clock_init(P1VideoClock *vclock);

// Configure the video clock. Calls into the subclass config method.
void p1_video_clock_config(P1VideoClock *vclock, P1Config *cfg);

// Callback for video clocks to emit ticks.
void p1_video_clock_tick(P1VideoClock *vclock, int64_t time);


// Video sources produce images on each clock tick. Several may be added to a
// context, to be combined into a single output image.

struct _P1VideoSource {
    P1Source super;

    // Texture name. The source need not touch this.
    uint32_t texture;

    // Top left and bottom right coordinates of where to place frames in the
    // output image. These are in the range [-1, +1].
    float x1, y1, x2, y2;
    // Top left and bottom right coordinates of the area in the frame to grab,
    // used to achieve clipping. These are in the range [0, 1].
    float u1, v1, u2, v2;

    // Produce the latest frame using p1_video_frame.
    // This is called from the clock thread.
    bool (*frame)(P1VideoSource *source);
};

// Subclasses should call into this from the initializer.
bool p1_video_source_init(P1VideoSource *vsrc);

// Configure the video source. Calls into the subclass config method.
void p1_video_source_config(P1VideoSource *vsrc, P1Config *cfg);

// Callback for video sources to provide frame data.
void p1_video_source_frame(P1VideoSource *vsrc, int width, int height, void *data);


// Audio sources produce buffers as they become available, using
// p1_audio_buffer. Several may be added to a context, to be mixed into a
// single output stream. Audio sources may emit buffers from any thread.

struct _P1AudioSource {
    P1Source super;

    // In the range [0, 1].
    float volume;
};

// Subclasses should call into this from the initializer.
bool p1_audio_source_init(P1AudioSource *asrc);

// Configure the audio source. Calls into the subclass config method.
void p1_audio_source_config(P1AudioSource *asrc, P1Config *cfg);

// Callback for audio sources to provide audio buffer data.
void p1_audio_source_buffer(P1AudioSource *asrc, int64_t time, float *in, size_t samples);


// Fixed audio mixer element.

struct _P1Audio {
    P1Object super;

    // The source list. Can be modified while running, as long as the lock is
    // held. Use the p1_list_* functions for convenience.
    P1ListNode sources;
};


// Fixed video mixer element.

struct _P1Video {
    P1Object super;

    // The video clock. Only modify this when the video mixer is idle.
    P1VideoClock *clock;

    // The source list. Can be modified while running, as long as the lock is
    // held. Use the p1_list_* functions for convenience.
    P1ListNode sources;

    // Function that will be called for each frame with raw RGBA data.
    // Note that this function is called on the clock thread.
    P1VideoPreviewCallback preview_fn;
    void *preview_user_data;
};


// Fixed stream connection element.

struct _P1Connection {
    P1Object super;
};


// Context that encapsulates everything else.

struct _P1Context {
    P1Object super;

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

// Create a new context.
P1Context *p1_create();
// Configure a context based on the given configuration.
void p1_config(P1Context *ctx, P1Config *cfg);

// Free all resources related to the context, and optionally other objects.
void p1_free(P1Context *ctx, P1FreeOptions options);

// Start running with the current configuration.
bool p1_start(P1Context *ctx);
// Stop all processing and all sources.
void p1_stop(P1Context *ctx, P1StopOptions options);

// Read a P1Notification. This method will block.
void p1_read(P1Context *ctx, P1Notification *out);
// Returns a file descriptor that can be used with poll(2) or select(2),
// to determine if p1_read will not block on the next call.
int p1_fd(P1Context *ctx);


// Logging functions.
void p1_log(P1Object *obj, P1LogLevel level, const char *fmt, ...) __printflike(3, 4);
void p1_logv(P1Object *obj, P1LogLevel level, const char *fmt, va_list args) __printflike(3, 0);

// Notification helper.
void _p1_notify(P1Notification notification);


// Platform-specific functionality.

#if __APPLE__
#   include <TargetConditionals.h>
#   if TARGET_OS_MAC
#       include "osx/p1stream_osx.h"
#   else
#       error Unsupported platform
#   endif
#else
#   error Unsupported platform
#endif

#endif
