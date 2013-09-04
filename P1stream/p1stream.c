#include "p1stream_priv.h"

#include <unistd.h>
#include <sys/poll.h>

typedef enum _P1Action P1Action;

static void p1_log_default(P1Context *ctx, P1LogLevel level, const char *fmt, va_list args, void *user_data);
static void *p1_ctrl_main(void *data);
static bool p1_ctrl_progress(P1Context *ctx);
static P1Action p1_ctrl_determine_action(P1Context *ctx, P1State state, P1TargetState target);
static void p1_ctrl_comm(P1ContextFull *ctxf);
static void p1_ctrl_log_notification(P1Context *ctx, P1Notification *notification);

// Based on state and target, one of these actions is taken.
enum _P1Action {
    P1_ACTION_NONE      = 0,
    P1_ACTION_WAIT      = 1,    // Same as none, but don't exit yet if stopping.
    P1_ACTION_START     = 2,
    P1_ACTION_STOP      = 3,
    P1_ACTION_REMOVE    = 4
};


P1Context *p1_create(P1Config *cfg, P1ConfigSection *sect)
{
    P1ConfigSection *video_sect = cfg->get_section(cfg, sect, "video");
    P1ConfigSection *audio_sect = cfg->get_section(cfg, sect, "audio");
    P1ConfigSection *stream_sect = cfg->get_section(cfg, sect, "stream");

    P1ContextFull *ctxf = calloc(1,
        sizeof(P1ContextFull) + sizeof(P1VideoFull) +
        sizeof(P1AudioFull) + sizeof(P1ConnectionFull));
    P1VideoFull *videof = (P1VideoFull *) (ctxf + 1);
    P1AudioFull *audiof = (P1AudioFull *) (videof + 1);
    P1ConnectionFull *connf = (P1ConnectionFull *) (audiof + 1);

    P1Context *ctx = (P1Context *) ctxf;
    P1Video *video = (P1Video *) videof;
    P1Audio *audio = (P1Audio *) audiof;
    P1Connection *conn = (P1Connection *) connf;

    ctx->video = video;
    ctx->audio = audio;
    ctx->conn = conn;

    video->ctx = ctx;
    audio->ctx = ctx;
    conn->ctx = ctx;

    ctx->log_level = P1_LOG_INFO;
    ctx->log_fn = p1_log_default;

    int ret;

    ret = pipe(ctxf->ctrl_pipe);
    assert(ret == 0);
    ret = pipe(ctxf->user_pipe);
    assert(ret == 0);

    mach_timebase_info(&ctxf->timebase);

    p1_video_init(videof, cfg, video_sect);
    p1_audio_init(audiof, cfg, audio_sect);
    p1_conn_init(connf, cfg, stream_sect);

    return ctx;
}

void p1_free(P1Context *ctx, P1FreeOptions options)
{
    // FIXME
}

void p1_start(P1Context *_ctx)
{
    P1ContextFull *ctx = (P1ContextFull *) _ctx;

    if (_ctx->state != P1_STATE_IDLE)
        return;

    p1_set_state(_ctx, P1_OTYPE_CONTEXT, _ctx, P1_STATE_STARTING);

    int ret = pthread_create(&ctx->ctrl_thread, NULL, p1_ctrl_main, ctx);
    assert(ret == 0);
}

void p1_stop(P1Context *_ctx)
{
    P1ContextFull *ctx = (P1ContextFull *) _ctx;

    if (_ctx->state != P1_STATE_RUNNING)
        return;

    // FIXME: Lock for this? Especially if the context thread can eventually
    // stop itself for whatever reason.
    p1_set_state(_ctx, P1_OTYPE_CONTEXT, _ctx, P1_STATE_STOPPING);

    int ret = pthread_join(ctx->ctrl_thread, NULL);
    assert(ret == 0);
}

void p1_read(P1Context *_ctx, P1Notification *out)
{
    P1ContextFull *ctx = (P1ContextFull *) _ctx;

    ssize_t size = sizeof(P1Notification);
    ssize_t ret = read(ctx->user_pipe[0], out, size);
    assert(ret == size);
}

int p1_fd(P1Context *_ctx)
{
    P1ContextFull *ctx = (P1ContextFull *) _ctx;

    return ctx->user_pipe[0];
}

void _p1_notify(P1Context *_ctx, P1Notification notification)
{
    P1ContextFull *ctx = (P1ContextFull *) _ctx;

    ssize_t size = sizeof(P1Notification);
    ssize_t ret = write(ctx->ctrl_pipe[1], &notification, size);
    assert(ret == size);
}

void p1_log(P1Context *ctx, P1LogLevel level, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    p1_logv(ctx, level, fmt, args);
    va_end(args);
}

void p1_logv(P1Context *ctx, P1LogLevel level, const char *fmt, va_list args)
{
    if (level <= ctx->log_level)
        ctx->log_fn(ctx, level, fmt, args, ctx->log_user_data);
}

// Default log function.
static void p1_log_default(P1Context *ctx, P1LogLevel level, const char *fmt, va_list args, void *user_data)
{
    const char *pre;
    switch (level) {
        case P1_LOG_ERROR:      pre = "error";      break;
        case P1_LOG_WARNING:    pre = "warning";    break;
        case P1_LOG_INFO:       pre = "info";       break;
        case P1_LOG_DEBUG:      pre = "debug";      break;
        default: pre = "error"; break;
    }
    fprintf(stderr, "[%s] ", pre);
    vfprintf(stderr, fmt, args);
}

// The control thread main loop.
static void *p1_ctrl_main(void *data)
{
    P1Context *ctx = (P1Context *) data;
    P1ContextFull *ctxf = (P1ContextFull *) ctx;

    p1_set_state(ctx, P1_OTYPE_CONTEXT, ctx, P1_STATE_RUNNING);

    // Loop until we hit the exit condition. This only happens when we're
    // stopping and are no longer waiting on objects to stop.
    do {
        p1_ctrl_comm(ctxf);
    } while (p1_ctrl_progress(ctx));

    p1_set_state(ctx, P1_OTYPE_CONTEXT, ctx, P1_STATE_IDLE);
    p1_ctrl_comm(ctxf);

    return NULL;
}

// Handle communication on pipes.
static void p1_ctrl_comm(P1ContextFull *ctxf)
{
    P1Context *ctx = (P1Context *) ctxf;
    int i_ret;
    struct pollfd fd = {
        .fd = ctxf->ctrl_pipe[0],
        .events = POLLIN
    };

    P1Notification notification;
    ssize_t size = sizeof(P1Notification);
    ssize_t s_ret;

    // Wait indefinitely for the next notification.
    do {
        i_ret = poll(&fd, 1, -1);
        assert(i_ret != -1);
    } while (i_ret == 0);

    do {
        // Read the notification.
        s_ret = read(fd.fd, &notification, size);
        assert(s_ret == size);

        // Log notification.
        p1_ctrl_log_notification(ctx, &notification);

        // Pass it on to the user.
        s_ret = write(ctxf->user_pipe[1], &notification, size);
        assert(s_ret == size);

        // Flush other notifications.
        i_ret = poll(&fd, 1, 0);
        assert(i_ret != -1);
    } while (i_ret == 1);
}

// Check all objects and try to progress state.
static bool p1_ctrl_progress(P1Context *ctx)
{
    P1Video *video = ctx->video;
    P1VideoFull *videof = (P1VideoFull *) video;
    P1Audio *audio = ctx->audio;
    P1AudioFull *audiof = (P1AudioFull *) audio;
    P1Connection *conn = ctx->conn;
    P1ConnectionFull *connf = (P1ConnectionFull *) conn;
    P1ListNode *head;
    P1ListNode *node;
    P1TargetState target;
    bool wait = false;

// After an action, check if we need to wait.
#define P1_CHECK_WAIT(_obj)                                 \
    if ((_obj)->state == P1_STATE_STARTING ||               \
        (_obj)->state == P1_STATE_STOPPING)                 \
        wait = true;

// Common action handling for plugins.
#define P1_PLUGIN_COMMON_ACTIONS(_obj)                      \
    case P1_ACTION_WAIT:                                    \
        wait = true;                                        \
        break;                                              \
    case P1_ACTION_START:                                   \
        (_obj)->ctx = ctx;                                  \
        (_obj)->start(_obj);                                \
        P1_CHECK_WAIT(_obj);                                \
        break;                                              \
    case P1_ACTION_STOP:                                    \
        (_obj)->stop(_obj);                                 \
        P1_CHECK_WAIT(_obj);                                \
        break;

// Common action handling for fixed components
#define P1_COMPONENT_COMMON_ACTIONS(_obj, _start, _stop)    \
    case P1_ACTION_WAIT:                                    \
        wait = true;                                        \
        break;                                              \
    case P1_ACTION_START:                                   \
        (_start)(_obj);                                     \
        P1_CHECK_WAIT(&(_obj)->super);                      \
        break;                                              \
    case P1_ACTION_STOP:                                    \
        (_stop)(_obj);                                      \
        P1_CHECK_WAIT(&(_obj)->super);                      \
        break;

// Source remove action handling.
#define P1_SOURCE_REMOVE_ACTION(_obj)                       \
    case P1_ACTION_REMOVE:                                  \
        p1_list_remove(_obj);                               \
        (_obj)->free(_obj);                                 \
        break;

// Short-hand for empty default case.
#define P1_EMPTY_DEFAULT                                    \
    default:                                                \
        break;

    // Progress clock. Clock target state is tied to the context.
    P1VideoClock *clock = ctx->video->clock;
    target = ctx->state == P1_STATE_STOPPING ? P1_TARGET_IDLE : P1_TARGET_RUNNING;
    switch (p1_ctrl_determine_action(ctx, clock->state, target)) {
        P1_PLUGIN_COMMON_ACTIONS(clock)
        P1_EMPTY_DEFAULT
    }

    // Progress video sources.
    pthread_mutex_lock(&video->lock);
    head = &video->sources;
    p1_list_iterate(head, node) {
        P1Source *src = (P1Source *) node;
        switch (p1_ctrl_determine_action(ctx, src->state, src->target)) {
            P1_PLUGIN_COMMON_ACTIONS(src)
            P1_SOURCE_REMOVE_ACTION(src)
            P1_EMPTY_DEFAULT
        }
    }
    pthread_mutex_unlock(&video->lock);

    // Progress audio sources.
    pthread_mutex_lock(&audio->lock);
    head = &audio->sources;
    p1_list_iterate(head, node) {
        P1Source *src = (P1Source *) node;
        switch (p1_ctrl_determine_action(ctx, src->state, src->target)) {
            P1_PLUGIN_COMMON_ACTIONS(src)
            P1_SOURCE_REMOVE_ACTION(src)
            P1_EMPTY_DEFAULT
        }
    }
    pthread_mutex_unlock(&audio->lock);

    // Progress video mixer. We need a running clock for this.
    if (clock->state != P1_STATE_RUNNING)
        target = P1_TARGET_IDLE;
    else
        target = video->target;
    switch (p1_ctrl_determine_action(ctx, video->state, target)) {
        P1_COMPONENT_COMMON_ACTIONS(videof, p1_video_start, p1_video_stop)
        P1_EMPTY_DEFAULT
    }

    // Progress audio mixer.
    switch (p1_ctrl_determine_action(ctx, audio->state, audio->target)) {
        P1_COMPONENT_COMMON_ACTIONS(audiof, p1_audio_start, p1_audio_stop)
        P1_EMPTY_DEFAULT
    }

    // Progress stream connnection. Delay until everything else is running.
    if (!wait) {
        switch (p1_ctrl_determine_action(ctx, conn->state, conn->target)) {
            P1_COMPONENT_COMMON_ACTIONS(connf, p1_conn_start, p1_conn_stop)
            P1_EMPTY_DEFAULT
        }
    }

    // Return whether we can exit.
    return wait || ctx->state != P1_STATE_STOPPING;
}

// Determine action to take on an object.
static P1Action p1_ctrl_determine_action(P1Context *ctx, P1State state, P1TargetState target)
{
    // We need to wait on the transition to finish.
    if (state == P1_STATE_STARTING || state == P1_STATE_STOPPING)
        return P1_ACTION_WAIT;

    // If the context is stopping, override target.
    // Make sure we preserve remove targets.
    if (target == P1_TARGET_RUNNING && ctx->state == P1_STATE_STOPPING)
        target = P1_TARGET_IDLE;

    // Take steps towards target.
    if (target == P1_TARGET_RUNNING) {
        if (state == P1_STATE_IDLE)
            return P1_ACTION_START;

        return P1_ACTION_NONE;
    }
    else {
        if (state == P1_STATE_RUNNING)
            return P1_ACTION_STOP;

        if (target == P1_TARGET_REMOVE && state == P1_TARGET_IDLE)
            return P1_ACTION_REMOVE;

        return P1_ACTION_NONE;
    }
}

// Log a notification.
static void p1_ctrl_log_notification(P1Context *ctx, P1Notification *notification)
{
    const char *action;
    switch (notification->type) {
        case P1_NTYPE_STATE_CHANGE:
            switch (notification->state_change.state) {
                case P1_STATE_IDLE:     action = "is idle";     break;
                case P1_STATE_STARTING: action = "is starting"; break;
                case P1_STATE_RUNNING:  action = "is running";  break;
                case P1_STATE_STOPPING: action = "is stopping"; break;
                default: return;
            }
            break;
        case P1_NTYPE_TARGET_CHANGE:
            switch (notification->target_change.target) {
                case P1_TARGET_RUNNING: action = "target running";  break;
                case P1_TARGET_IDLE:    action = "target idle";     break;
                case P1_TARGET_REMOVE:  action = "target remove";   break;
                default: return;
            }
            break;
        default: return;
    }

    const char *obj_descr;
    switch (notification->object_type) {
        case P1_OTYPE_CONTEXT:      obj_descr = "context";      break;
        case P1_OTYPE_VIDEO:        obj_descr = "video mixer";  break;
        case P1_OTYPE_AUDIO:        obj_descr = "audio mixer";  break;
        case P1_OTYPE_CONNECTION:   obj_descr = "connection";   break;
        case P1_OTYPE_VIDEO_CLOCK:  obj_descr = "video clock";  break;
        case P1_OTYPE_VIDEO_SOURCE: obj_descr = "video source"; break;
        case P1_OTYPE_AUDIO_SOURCE: obj_descr = "audio source"; break;
        default: return;
    }

    p1_log(ctx, P1_LOG_INFO, "%s %p %s\n", obj_descr, notification->object, action);
}
